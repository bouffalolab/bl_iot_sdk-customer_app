/*
 * Copyright (c) 2016-2026 Bouffalolab.
 *
 * This file is part of
 *     *** Bouffalolab Software Dev Kit ***
 *      (see www.bouffalolab.com).
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright notice,
 *      this list of conditions and the following disclaimer in the documentation
 *      and/or other materials provided with the distribution.
 *   3. Neither the name of Bouffalo Lab nor the names of its contributors
 *      may be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Minimal Thread Credential Sharing pull prototype.
 *
 * This mirrors the host raw_dataset_pull commissioner-cli prototype:
 * one-time code/ePSKc -> DTLS EC-JPAKE to a Border Agent -> MGMT_ACTIVE_GET
 * -> raw Active Dataset TLVs -> optional local otDatasetSetActiveTlvs().
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include <cli.h>

#include <lwip/inet.h>
#include <lwip/ip_addr.h>
#include <lwip/netif.h>
#include <lwip/sockets.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ciphersuites.h>

#include <openthread/dataset.h>
#include <openthread/error.h>
#include <openthread/ip6.h>
#include <openthread/platform/entropy.h>
#include <openthread/thread.h>
#include <openthread_port.h>
#include <openthread_br.h>

struct netif *otbr_getInfraNetif(void);

#define TCS_CODE_MAX_LEN 64
#define TCS_HOST_MAX_LEN 80
#define TCS_COAP_MAX_LEN 512
#define TCS_COAP_TOKEN_LEN 8
#define TCS_DATASET_TLV_MAX_LEN OT_OPERATIONAL_DATASET_MAX_LENGTH
#define TCS_TASK_STACK_DEPTH 4096
#define TCS_TASK_PRIORITY 10
#define TCS_DTLS_READ_TIMEOUT_MS 20000
#define TCS_DTLS_ZERO_READ_TIMEOUT_MS 100
#define TCS_DTLS_HANDSHAKE_DEADLINE_MS 70000
#define TCS_ENTROPY_MIN_THRESHOLD 16
#define TCS_MDNS_SERVICE "_meshcop-e"
#define TCS_MDNS_PROTO "_udp"
#define TCS_MDNS_SCAN_TIMEOUT_MS 3000
#define TCS_MDNS_SCAN_MAX_RESULTS 1

typedef enum {
    TCS_MODE_PRINT = 0,
    TCS_MODE_APPLY,
    TCS_MODE_APPLY_START,
} tcs_mode_t;

typedef struct {
    char       code[TCS_CODE_MAX_LEN + 1];
    char       host[TCS_HOST_MAX_LEN + 1];
    uint16_t   port;
    tcs_mode_t mode;
} tcs_pull_args_t;

typedef struct {
    uint32_t timeout_ms;
} tcs_scan_args_t;

typedef struct {
    TickType_t start_tick;
    uint32_t   intermediate_ms;
    uint32_t   final_ms;
    bool       active;
} tcs_dtls_timer_t;

typedef struct {
    int fd;
} tcs_net_context_t;

typedef int esp_err_t;

typedef enum {
    MDNS_IP_PROTOCOL_V4 = 0,
#if LWIP_IPV6
    MDNS_IP_PROTOCOL_V6,
#endif
    MDNS_IP_PROTOCOL_MAX
} mdns_ip_protocol_t;

typedef struct {
    const char *key;
    const char *value;
    uint32_t    valuelen;
} mdns_txt_item_t;

typedef struct mdns_ip_addr_s {
    ip_addr_t              addr;
    struct mdns_ip_addr_s *next;
} mdns_ip_addr_t;

typedef struct mdns_result_s {
    struct mdns_result_s *next;
    void                 *nif;
    uint32_t              ttl;
    mdns_ip_protocol_t    ip_protocol;
    char                 *instance_name;
    char                 *service_type;
    char                 *proto;
    char                 *hostname;
    uint16_t              port;
    mdns_txt_item_t      *txt;
    uint8_t              *txt_value_len;
    size_t                txt_count;
    mdns_ip_addr_t       *addr;
} mdns_result_t;

extern esp_err_t mdns_query_ptr(const char *service_type, const char *proto, uint32_t timeout,
                                size_t max_results, mdns_result_t **results);
extern esp_err_t mdns_query_srv(const char *instance_name, const char *service_type, const char *proto,
                                uint32_t timeout, mdns_result_t **result);
extern void mdns_query_results_free(mdns_result_t *results);

static TaskHandle_t sTcsTaskHandle;
static tcs_pull_args_t sTcsArgs;
static tcs_scan_args_t sTcsScanArgs;

static const int sTcsCipherSuites[] = {
    MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8,
    0,
};

static int tcs_mbedtls_entropy_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    (void)data;

    if (output == NULL || olen == NULL || len > UINT16_MAX) {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }

    if (otPlatEntropyGet(output, (uint16_t)len) != OT_ERROR_NONE) {
        return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    }

    *olen = len;
    return 0;
}

static const uint8_t sTcsActiveDatasetTypes[] = {
    14, /* Active Timestamp */
    0,  /* Channel */
    53, /* Channel Mask */
    2,  /* Extended PAN ID */
    7,  /* Mesh Local Prefix */
    5,  /* Network Key */
    3,  /* Network Name */
    1,  /* PAN ID */
    4,  /* PSKc */
    12, /* Security Policy */
};

static void tcs_print_usage(void)
{
    printf("Usage:\r\n");
    printf("  tcs_pull <one_time_code> <border_agent_ip> <border_agent_port> [print|apply|apply-start]\r\n");
    printf("Notes:\r\n");
    printf("  default mode is print; code is not echoed in logs\r\n");
    printf("  border_agent_ip must be an IPv4/IPv6 literal; IPv6 link-local may use %%<ifindex>\r\n");
    printf("  apply-start disables Thread/IP6 before setting TLVs, then starts Thread again\r\n");
}

static void tcs_print_scan_usage(void)
{
    printf("Usage:\r\n");
    printf("  tcs_scan [timeout_ms]\r\n");
    printf("Notes:\r\n");
    printf("  scans " TCS_MDNS_SERVICE "." TCS_MDNS_PROTO ".local Border Agents on the infra network\r\n");
    printf("  returns the first endpoint; timeout_ms is the total scan budget\r\n");
    printf("  default: timeout_ms=%u\r\n", TCS_MDNS_SCAN_TIMEOUT_MS);
}

static void tcs_print_hex(const char *prefix, const uint8_t *buf, size_t len)
{
    printf("%s", prefix);
    for (size_t i = 0; i < len; i++) {
        printf("%02x", buf[i]);
    }
    printf("\r\n");
}

static void tcs_print_hex_bytes(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        printf("%02x", buf[i]);
    }
}

static void tcs_task_mark_done(void)
{
    sTcsTaskHandle = NULL;
}

static uint32_t tcs_scan_remaining_ms(TickType_t started_tick, uint32_t timeout_ms)
{
    uint32_t elapsed_ms = (uint32_t)(xTaskGetTickCount() - started_tick) * portTICK_PERIOD_MS;

    return elapsed_ms < timeout_ms ? timeout_ms - elapsed_ms : 0;
}

static size_t tcs_mdns_txt_value_len(const mdns_result_t *result, size_t index)
{
    if (result == NULL || result->txt == NULL || index >= result->txt_count) {
        return 0;
    }

    if (result->txt_value_len != NULL) {
        return result->txt_value_len[index];
    }

    return result->txt[index].valuelen;
}

static const uint8_t *tcs_mdns_find_txt(const mdns_result_t *result, const char *key, size_t *value_len)
{
    if (value_len != NULL) {
        *value_len = 0;
    }

    if (result == NULL || result->txt == NULL || key == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < result->txt_count; i++) {
        if (result->txt[i].key != NULL && strcmp(result->txt[i].key, key) == 0) {
            if (value_len != NULL) {
                *value_len = tcs_mdns_txt_value_len(result, i);
            }
            return (const uint8_t *)result->txt[i].value;
        }
    }

    return NULL;
}

static bool tcs_bytes_are_printable(const uint8_t *value, size_t value_len)
{
    if (value == NULL) {
        return false;
    }

    for (size_t i = 0; i < value_len; i++) {
        if (value[i] < 0x20 || value[i] > 0x7e) {
            return false;
        }
    }

    return true;
}

static void tcs_print_txt_text(const uint8_t *value, size_t value_len)
{
    if (value == NULL || !tcs_bytes_are_printable(value, value_len)) {
        return;
    }

    for (size_t i = 0; i < value_len; i++) {
        printf("%c", value[i]);
    }
}

static void tcs_print_mdns_summary(uint32_t result_index, const char *instance,
                                   const mdns_result_t *endpoint_result,
                                   const mdns_result_t *txt_result)
{
    const uint8_t *network_name;
    const uint8_t *xpanid;
    size_t network_name_len = 0;
    size_t xpanid_len = 0;

    network_name = tcs_mdns_find_txt(txt_result, "nn", &network_name_len);
    xpanid = tcs_mdns_find_txt(txt_result, "xp", &xpanid_len);

    printf("TCS_MDNS_RESULT index=%u instance=%s host=%s port=%u ttl=%u network_name=",
           (unsigned)result_index,
           instance != NULL ? instance : "",
           endpoint_result != NULL && endpoint_result->hostname != NULL ? endpoint_result->hostname : "",
           endpoint_result != NULL ? (unsigned)endpoint_result->port : 0,
           endpoint_result != NULL ? (unsigned)endpoint_result->ttl : 0);

    tcs_print_txt_text(network_name, network_name_len);
    printf(" xpanid=");
    if (xpanid != NULL && xpanid_len > 0) {
        tcs_print_hex_bytes(xpanid, xpanid_len);
    }
    printf("\r\n");
}

static void tcs_print_mdns_endpoint(uint32_t result_index, uint32_t address_index,
                                    const mdns_ip_addr_t *address, void *netif, uint16_t port)
{
    char ipbuf[IPADDR_STRLEN_MAX];
    uint8_t netif_index = 0;

    if (address == NULL || ipaddr_ntoa_r(&address->addr, ipbuf, sizeof(ipbuf)) == NULL) {
        return;
    }

    if (netif != NULL) {
        netif_index = netif_get_index((struct netif *)netif);
    }

    printf("TCS_MDNS_ENDPOINT index=%u addr_index=%u ip=%s",
           (unsigned)result_index, (unsigned)address_index, ipbuf);
#if LWIP_IPV6
    if (IP_IS_V6_VAL(address->addr) && ip6_addr_islinklocal(ip_2_ip6(&address->addr)) && netif_index != 0) {
        printf("%%%u", (unsigned)netif_index);
    }
#endif
    printf(" port=%u type=%s\r\n", (unsigned)port, IP_IS_V6_VAL(address->addr) ? "IPv6" : "IPv4");
}

static void tcs_print_mdns_txt(uint32_t result_index, const mdns_result_t *result)
{
    if (result == NULL || result->txt == NULL || result->txt_count == 0) {
        return;
    }

    for (size_t i = 0; i < result->txt_count; i++) {
        size_t value_len = tcs_mdns_txt_value_len(result, i);
        const uint8_t *value = (const uint8_t *)result->txt[i].value;

        printf("TCS_MDNS_TXT index=%u item=%u key=%s value=",
               (unsigned)result_index, (unsigned)i,
               result->txt[i].key != NULL ? result->txt[i].key : "");
        if (tcs_bytes_are_printable(value, value_len)) {
            tcs_print_txt_text(value, value_len);
        }
        printf(" value_hex=");
        if (value != NULL && value_len > 0) {
            tcs_print_hex_bytes(value, value_len);
        }
        printf("\r\n");
    }
}

static void tcs_dtls_set_timer(void *ctx, uint32_t intermediate_ms, uint32_t final_ms)
{
    tcs_dtls_timer_t *timer = (tcs_dtls_timer_t *)ctx;

    timer->start_tick = xTaskGetTickCount();
    timer->intermediate_ms = intermediate_ms;
    timer->final_ms = final_ms;
    timer->active = final_ms != 0;
}

static int tcs_dtls_get_timer(void *ctx)
{
    tcs_dtls_timer_t *timer = (tcs_dtls_timer_t *)ctx;
    uint32_t elapsed_ms;

    if (!timer->active) {
        return -1;
    }

    elapsed_ms = (uint32_t)((xTaskGetTickCount() - timer->start_tick) * portTICK_PERIOD_MS);
    if (elapsed_ms >= timer->final_ms) {
        return 2;
    }

    if (elapsed_ms >= timer->intermediate_ms) {
        return 1;
    }

    return 0;
}

static const char *tcs_mode_to_string(tcs_mode_t mode)
{
    switch (mode) {
        case TCS_MODE_PRINT:
            return "print";
        case TCS_MODE_APPLY:
            return "apply";
        case TCS_MODE_APPLY_START:
            return "apply-start";
        default:
            return "unknown";
    }
}

static int tcs_parse_mode(const char *arg, tcs_mode_t *mode)
{
    if (arg == NULL || strcmp(arg, "print") == 0) {
        *mode = TCS_MODE_PRINT;
        return 0;
    }

    if (strcmp(arg, "apply") == 0) {
        *mode = TCS_MODE_APPLY;
        return 0;
    }

    if (strcmp(arg, "apply-start") == 0) {
        *mode = TCS_MODE_APPLY_START;
        return 0;
    }

    return -1;
}

static bool tcs_parse_uint32(const char *arg, uint32_t max_value, uint32_t *value)
{
    uint32_t parsed = 0;

    if (arg == NULL || value == NULL || *arg == '\0') {
        return false;
    }

    for (; *arg != '\0'; arg++) {
        uint32_t digit;

        if (*arg < '0' || *arg > '9') {
            return false;
        }

        digit = (uint32_t)(*arg - '0');
        if (digit > max_value || parsed > (max_value - digit) / 10) {
            return false;
        }
        parsed = parsed * 10 + digit;
    }

    *value = parsed;
    return true;
}

static bool tcs_strip_scope(char *host, uint32_t *scope_id)
{
    char *scope = strchr(host, '%');

    *scope_id = 0;
    if (scope == NULL) {
        return true;
    }

    *scope++ = '\0';
    return tcs_parse_uint32(scope, UINT32_MAX, scope_id);
}

static void tcs_net_init(tcs_net_context_t *net)
{
    net->fd = -1;
}

static void tcs_net_free(tcs_net_context_t *net)
{
    if (net->fd >= 0) {
        close(net->fd);
        net->fd = -1;
    }
}

static int tcs_net_send(void *ctx, const unsigned char *buf, size_t len)
{
    tcs_net_context_t *net = (tcs_net_context_t *)ctx;
    int ret;

    if (net->fd < 0) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    ret = send(net->fd, buf, len, 0);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
            return MBEDTLS_ERR_SSL_WANT_WRITE;
        }
        return MBEDTLS_ERR_NET_SEND_FAILED;
    }

    return ret;
}

static int tcs_net_recv(void *ctx, unsigned char *buf, size_t len)
{
    tcs_net_context_t *net = (tcs_net_context_t *)ctx;
    int ret;

    if (net->fd < 0) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    ret = recv(net->fd, buf, len, 0);
    if (ret < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        if (errno == EPIPE || errno == ECONNRESET) {
            return MBEDTLS_ERR_NET_CONN_RESET;
        }
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    return ret;
}

static int tcs_net_recv_timeout(void *ctx, unsigned char *buf, size_t len, uint32_t timeout_ms)
{
    tcs_net_context_t *net = (tcs_net_context_t *)ctx;
    struct timeval tv;
    fd_set read_fds;
    int ret;

    if (net->fd < 0) {
        return MBEDTLS_ERR_NET_INVALID_CONTEXT;
    }

    FD_ZERO(&read_fds);
    FD_SET(net->fd, &read_fds);

    if (timeout_ms == 0) {
        timeout_ms = TCS_DTLS_ZERO_READ_TIMEOUT_MS;
    }

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    ret = select(net->fd + 1, &read_fds, NULL, NULL, &tv);
    if (ret == 0) {
        return MBEDTLS_ERR_SSL_TIMEOUT;
    }
    if (ret < 0) {
        if (errno == EINTR) {
            return MBEDTLS_ERR_SSL_WANT_READ;
        }
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }

    return tcs_net_recv(ctx, buf, len);
}

static int tcs_socket_connect(tcs_net_context_t *net, const char *host_arg, uint16_t port)
{
    char host[TCS_HOST_MAX_LEN + 1];
    ip_addr_t ipaddr;
    uint32_t scope_id = 0;
    int fd = -1;

    strncpy(host, host_arg, sizeof(host) - 1);
    host[sizeof(host) - 1] = '\0';
    if (!tcs_strip_scope(host, &scope_id)) {
        return -1;
    }

    if (ipaddr_aton(host, &ipaddr) == 0) {
        return -1;
    }

    if (IP_IS_V6_VAL(ipaddr)) {
        struct sockaddr_in6 addr6;

        fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0) {
            return -1;
        }

        memset(&addr6, 0, sizeof(addr6));
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(port);
        inet6_addr_from_ip6addr(&addr6.sin6_addr, ip_2_ip6(&ipaddr));

        if (ip6_addr_islinklocal(ip_2_ip6(&ipaddr))) {
            struct netif *infra = otbr_getInfraNetif();
            if (scope_id == 0 && infra != NULL) {
                scope_id = netif_get_index(infra);
            }
            addr6.sin6_scope_id = scope_id;
        }

        if (connect(fd, (struct sockaddr *)&addr6, sizeof(addr6)) != 0) {
            close(fd);
            return -1;
        }
    } else {
        struct sockaddr_in addr4;

        fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (fd < 0) {
            return -1;
        }

        memset(&addr4, 0, sizeof(addr4));
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(port);
        inet_addr_from_ip4addr(&addr4.sin_addr, ip_2_ip4(&ipaddr));

        if (connect(fd, (struct sockaddr *)&addr4, sizeof(addr4)) != 0) {
            close(fd);
            return -1;
        }
    }

    net->fd = fd;
    return 0;
}

static int tcs_append_option(uint8_t *buf, size_t buf_size, size_t *len, uint16_t *last_option,
                             uint16_t option, const uint8_t *value, uint16_t value_len)
{
    uint16_t delta = option - *last_option;
    uint8_t delta_nibble;
    uint8_t len_nibble;

    if (option < *last_option || value_len >= 13 || delta >= 13 || *len + 1 + value_len > buf_size) {
        return -1;
    }

    delta_nibble = (uint8_t)delta;
    len_nibble = (uint8_t)value_len;

    buf[(*len)++] = (uint8_t)((delta_nibble << 4) | len_nibble);
    if (value_len > 0) {
        memcpy(&buf[*len], value, value_len);
        *len += value_len;
    }

    *last_option = option;
    return 0;
}

static int tcs_build_mgmt_active_get(uint8_t *buf, size_t buf_size, size_t *msg_len,
                                     uint16_t *message_id, uint8_t token[TCS_COAP_TOKEN_LEN])
{
    uint16_t last_option = 0;
    size_t len = 0;
    uint32_t ticks = (uint32_t)xTaskGetTickCount();

    if (buf_size < 4 + TCS_COAP_TOKEN_LEN) {
        return -1;
    }

    *message_id = (uint16_t)ticks;
    for (size_t i = 0; i < TCS_COAP_TOKEN_LEN; i++) {
        token[i] = (uint8_t)(ticks >> ((i % 4) * 8));
        token[i] ^= (uint8_t)(0x5a + i * 17);
    }

    buf[len++] = 0x40 | TCS_COAP_TOKEN_LEN; /* Version 1, Confirmable. */
    buf[len++] = 0x02; /* POST. */
    buf[len++] = (uint8_t)(*message_id >> 8);
    buf[len++] = (uint8_t)(*message_id & 0xff);
    memcpy(&buf[len], token, TCS_COAP_TOKEN_LEN);
    len += TCS_COAP_TOKEN_LEN;

    if (tcs_append_option(buf, buf_size, &len, &last_option, 11, (const uint8_t *)"c", 1) != 0 ||
        tcs_append_option(buf, buf_size, &len, &last_option, 11, (const uint8_t *)"ag", 2) != 0) {
        return -1;
    }

    if (len + 3 + sizeof(sTcsActiveDatasetTypes) > buf_size) {
        return -1;
    }

    buf[len++] = 0xff; /* Payload marker. */
    buf[len++] = 13;   /* MeshCoP Get TLV. */
    buf[len++] = (uint8_t)sizeof(sTcsActiveDatasetTypes);
    memcpy(&buf[len], sTcsActiveDatasetTypes, sizeof(sTcsActiveDatasetTypes));
    len += sizeof(sTcsActiveDatasetTypes);

    *msg_len = len;
    return 0;
}

static int tcs_parse_coap_payload(const uint8_t *buf, size_t len, const uint8_t token[TCS_COAP_TOKEN_LEN],
                                  uint8_t *code, const uint8_t **payload, size_t *payload_len)
{
    uint8_t tkl;
    size_t pos;

    *payload = NULL;
    *payload_len = 0;

    if (len < 4 || (buf[0] >> 6) != 1) {
        return -1;
    }

    tkl = buf[0] & 0x0f;
    if (tkl > TCS_COAP_TOKEN_LEN || len < (size_t)(4 + tkl)) {
        return -1;
    }

    *code = buf[1];
    if (tkl != TCS_COAP_TOKEN_LEN || memcmp(&buf[4], token, TCS_COAP_TOKEN_LEN) != 0) {
        return -1;
    }

    pos = 4 + tkl;
    while (pos < len) {
        uint8_t option_header;
        uint8_t delta;
        size_t opt_len;

        if (buf[pos] == 0xff) {
            pos++;
            *payload = &buf[pos];
            *payload_len = len - pos;
            return 0;
        }

        option_header = buf[pos++];
        delta = option_header >> 4;
        opt_len = option_header & 0x0f;

        if (delta == 13) {
            if (pos >= len) {
                return -1;
            }
            pos++;
        } else if (delta == 14) {
            if (pos + 1 >= len) {
                return -1;
            }
            pos += 2;
        } else if (delta == 15) {
            return -1;
        }

        if (opt_len == 13) {
            if (pos >= len) {
                return -1;
            }
            opt_len = (size_t)buf[pos++] + 13;
        } else if (opt_len == 14) {
            if (pos + 1 >= len) {
                return -1;
            }
            opt_len = ((size_t)buf[pos] << 8) | buf[pos + 1];
            opt_len += 269U;
            pos += 2;
        } else if (opt_len == 15) {
            return -1;
        }

        if (pos + opt_len > len) {
            return -1;
        }
        pos += opt_len;
    }

    return 0;
}

static otError tcs_validate_dataset_tlvs(const uint8_t *tlvs, size_t tlvs_len)
{
    otError err;
    otOperationalDataset dataset;
    otOperationalDatasetTlvs dataset_tlvs;

    if (tlvs_len == 0 || tlvs_len > sizeof(dataset_tlvs.mTlvs)) {
        return OT_ERROR_INVALID_ARGS;
    }

    memset(&dataset_tlvs, 0, sizeof(dataset_tlvs));
    memcpy(dataset_tlvs.mTlvs, tlvs, tlvs_len);
    dataset_tlvs.mLength = (uint8_t)tlvs_len;

    OT_THREAD_SAFE_RET(err, otDatasetParseTlvs(&dataset_tlvs, &dataset));
    mbedtls_platform_zeroize(&dataset, sizeof(dataset));
    mbedtls_platform_zeroize(&dataset_tlvs, sizeof(dataset_tlvs));
    return err;
}

static void tcs_restore_network_state(otInstance *instance, bool thread_was_enabled, bool ip6_was_enabled)
{
    bool thread_enabled;
    bool ip6_enabled;
    otError restore_err;

    OT_THREAD_SAFE(
        thread_enabled = otThreadGetDeviceRole(instance) != OT_DEVICE_ROLE_DISABLED;
        ip6_enabled = otIp6IsEnabled(instance);
    );

    if (!thread_was_enabled && thread_enabled) {
        OT_THREAD_SAFE_RET(restore_err, otThreadSetEnabled(instance, false));
        if (restore_err != OT_ERROR_NONE) {
            printf("TCS_PULL_ERROR state_restore target=thread_disabled status=%s\r\n",
                   otThreadErrorToString(restore_err));
        } else {
            thread_enabled = false;
        }
    }

    if (ip6_was_enabled && !ip6_enabled) {
        OT_THREAD_SAFE_RET(restore_err, otIp6SetEnabled(instance, true));
        if (restore_err != OT_ERROR_NONE) {
            printf("TCS_PULL_ERROR state_restore target=ip6_enabled status=%s\r\n",
                   otThreadErrorToString(restore_err));
        } else {
            ip6_enabled = true;
        }
    }

    if (thread_was_enabled && !thread_enabled) {
        OT_THREAD_SAFE_RET(restore_err, otThreadSetEnabled(instance, true));
        if (restore_err != OT_ERROR_NONE) {
            printf("TCS_PULL_ERROR state_restore target=thread_enabled status=%s\r\n",
                   otThreadErrorToString(restore_err));
        } else {
            thread_enabled = true;
        }
    }

    if (!ip6_was_enabled && ip6_enabled) {
        OT_THREAD_SAFE_RET(restore_err, otIp6SetEnabled(instance, false));
        if (restore_err != OT_ERROR_NONE) {
            printf("TCS_PULL_ERROR state_restore target=ip6_disabled status=%s\r\n",
                   otThreadErrorToString(restore_err));
        }
    }
}

static otError tcs_apply_dataset_tlvs(const uint8_t *tlvs, size_t tlvs_len, bool start_thread)
{
    otError err = OT_ERROR_NONE;
    otOperationalDatasetTlvs dataset_tlvs;
    otInstance *instance = otrGetInstance();
    bool thread_was_enabled = false;
    bool ip6_was_enabled = false;

    if (tlvs_len == 0 || tlvs_len > sizeof(dataset_tlvs.mTlvs)) {
        return OT_ERROR_INVALID_ARGS;
    }

    memset(&dataset_tlvs, 0, sizeof(dataset_tlvs));
    memcpy(dataset_tlvs.mTlvs, tlvs, tlvs_len);
    dataset_tlvs.mLength = (uint8_t)tlvs_len;

    if (start_thread) {
        OT_THREAD_SAFE(
            thread_was_enabled = otThreadGetDeviceRole(instance) != OT_DEVICE_ROLE_DISABLED;
            ip6_was_enabled = otIp6IsEnabled(instance);
        );

        if (thread_was_enabled) {
            OT_THREAD_SAFE_RET(err, otThreadSetEnabled(instance, false));
            if (err != OT_ERROR_NONE) {
                goto restore;
            }
        }

        if (ip6_was_enabled) {
            OT_THREAD_SAFE_RET(err, otIp6SetEnabled(instance, false));
            if (err != OT_ERROR_NONE) {
                goto restore;
            }
        }
    }

    OT_THREAD_SAFE_RET(err, otDatasetSetActiveTlvs(instance, &dataset_tlvs));
    if (err != OT_ERROR_NONE) {
        goto restore;
    }
    if (!start_thread) {
        goto exit;
    }

    OT_THREAD_SAFE_RET(err, otIp6SetEnabled(instance, true));
    if (err != OT_ERROR_NONE) {
        goto restore;
    }

    OT_THREAD_SAFE_RET(err, otThreadSetEnabled(instance, true));
    if (err != OT_ERROR_NONE) {
        goto restore;
    }
    goto exit;

restore:
    if (start_thread && err != OT_ERROR_NONE) {
        tcs_restore_network_state(instance, thread_was_enabled, ip6_was_enabled);
    }

exit:
    mbedtls_platform_zeroize(&dataset_tlvs, sizeof(dataset_tlvs));
    return err;
}

static int tcs_dtls_pull_dataset(const tcs_pull_args_t *args, uint8_t *dataset_tlvs, size_t *dataset_tlvs_len)
{
    int ret = -1;
    tcs_net_context_t server_fd;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    tcs_dtls_timer_t timer;
    uint8_t request[TCS_COAP_MAX_LEN];
    uint8_t response[TCS_COAP_MAX_LEN];
    uint8_t token[TCS_COAP_TOKEN_LEN];
    uint16_t message_id;
    size_t request_len;
    uint8_t response_code;
    const uint8_t *payload;
    size_t payload_len;
    const char *personalization = "bl702-tcs-pull";
    TickType_t handshake_start_tick;
    bool ssl_setup_done = false;

    tcs_net_init(&server_fd);
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    memset(&timer, 0, sizeof(timer));

    printf("TCS_PULL_BEGIN host=%s port=%u mode=%s code_len=%u\r\n",
           args->host, args->port, tcs_mode_to_string(args->mode), (unsigned)strlen(args->code));

    ret = tcs_socket_connect(&server_fd, args->host, args->port);
    if (ret != 0) {
        printf("TCS_PULL_ERROR socket_connect ret=%d\r\n", ret);
        goto exit;
    }
    printf("TCS_PULL_STEP socket_connected\r\n");

    ret = mbedtls_entropy_add_source(&entropy, tcs_mbedtls_entropy_poll, NULL,
                                     TCS_ENTROPY_MIN_THRESHOLD, MBEDTLS_ENTROPY_SOURCE_STRONG);
    if (ret != 0) {
        printf("TCS_PULL_ERROR entropy_add_source ret=-0x%x\r\n", -ret);
        goto exit;
    }

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char *)personalization, strlen(personalization));
    if (ret != 0) {
        printf("TCS_PULL_ERROR ctr_drbg_seed ret=-0x%x\r\n", -ret);
        goto exit;
    }

    ret = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        printf("TCS_PULL_ERROR ssl_config_defaults ret=-0x%x\r\n", -ret);
        goto exit;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ssl_conf_ciphersuites(&conf, sTcsCipherSuites);
    mbedtls_ssl_conf_min_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_max_version(&conf, MBEDTLS_SSL_MAJOR_VERSION_3, MBEDTLS_SSL_MINOR_VERSION_3);
    mbedtls_ssl_conf_handshake_timeout(&conf, 8000, 60000);
    mbedtls_ssl_conf_read_timeout(&conf, TCS_DTLS_READ_TIMEOUT_MS);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) {
        printf("TCS_PULL_ERROR ssl_setup ret=-0x%x\r\n", -ret);
        goto exit;
    }
    ssl_setup_done = true;

    ret = mbedtls_ssl_set_hs_ecjpake_password(&ssl, (const unsigned char *)args->code, strlen(args->code));
    if (ret != 0) {
        printf("TCS_PULL_ERROR set_ecjpake_password ret=-0x%x\r\n", -ret);
        goto exit;
    }

    mbedtls_ssl_set_mtu(&ssl, 1280);
    mbedtls_ssl_set_bio(&ssl, &server_fd, tcs_net_send, tcs_net_recv, tcs_net_recv_timeout);
    mbedtls_ssl_set_timer_cb(&ssl, &timer, tcs_dtls_set_timer, tcs_dtls_get_timer);

    printf("TCS_PULL_STEP dtls_handshake_start\r\n");
    handshake_start_tick = xTaskGetTickCount();
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            printf("TCS_PULL_ERROR dtls_handshake ret=-0x%x\r\n", -ret);
            goto exit;
        }

        if ((uint32_t)((xTaskGetTickCount() - handshake_start_tick) * portTICK_PERIOD_MS) >=
            TCS_DTLS_HANDSHAKE_DEADLINE_MS) {
            printf("TCS_PULL_ERROR dtls_handshake timeout_ms=%u\r\n", TCS_DTLS_HANDSHAKE_DEADLINE_MS);
            goto exit;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
    printf("TCS_PULL_STEP dtls_connected\r\n");

    if (tcs_build_mgmt_active_get(request, sizeof(request), &request_len, &message_id, token) != 0) {
        printf("TCS_PULL_ERROR build_mgmt_active_get\r\n");
        goto exit;
    }

    ret = mbedtls_ssl_write(&ssl, request, request_len);
    if (ret != (int)request_len) {
        printf("TCS_PULL_ERROR coap_write ret=%d expected=%u\r\n", ret, (unsigned)request_len);
        goto exit;
    }
    printf("TCS_PULL_STEP mgmt_active_get_sent mid=%u len=%u\r\n", message_id, (unsigned)request_len);

    ret = mbedtls_ssl_read(&ssl, response, sizeof(response));
    if (ret <= 0) {
        printf("TCS_PULL_ERROR coap_read ret=%d\r\n", ret);
        goto exit;
    }

    if (tcs_parse_coap_payload(response, (size_t)ret, token, &response_code, &payload, &payload_len) != 0) {
        printf("TCS_PULL_ERROR parse_coap_response len=%d\r\n", ret);
        goto exit;
    }

    printf("TCS_PULL_STEP mgmt_active_get_response code=0x%02x payload_len=%u\r\n",
           response_code, (unsigned)payload_len);

    if (response_code != 0x44 && response_code != 0x45) {
        printf("TCS_PULL_ERROR unexpected_coap_code=0x%02x\r\n", response_code);
        goto exit;
    }

    if (payload_len == 0 || payload_len > TCS_DATASET_TLV_MAX_LEN) {
        printf("TCS_PULL_ERROR invalid_dataset_len=%u\r\n", (unsigned)payload_len);
        goto exit;
    }

    memcpy(dataset_tlvs, payload, payload_len);
    *dataset_tlvs_len = payload_len;
    ret = 0;

exit:
    if (ssl_setup_done) {
        mbedtls_ssl_close_notify(&ssl);
    }
    tcs_net_free(&server_fd);
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    mbedtls_platform_zeroize(response, sizeof(response));
    mbedtls_platform_zeroize(token, sizeof(token));

    return ret;
}

static void tcs_pull_task(void *arg)
{
    tcs_pull_args_t args;
    uint8_t dataset_tlvs[TCS_DATASET_TLV_MAX_LEN];
    size_t dataset_tlvs_len = 0;
    otError ot_err;
    int ret;

    (void)arg;
    memcpy(&args, &sTcsArgs, sizeof(args));
    mbedtls_platform_zeroize(&sTcsArgs, sizeof(sTcsArgs));
    memset(dataset_tlvs, 0, sizeof(dataset_tlvs));

    ret = tcs_dtls_pull_dataset(&args, dataset_tlvs, &dataset_tlvs_len);
    if (ret != 0) {
        printf("TCS_PULL_END status=fail\r\n");
        goto exit;
    }

    if (args.mode == TCS_MODE_PRINT) {
        tcs_print_hex("TCS_ACTIVE_DATASET_TLV=", dataset_tlvs, dataset_tlvs_len);
    } else {
        printf("TCS_ACTIVE_DATASET_TLV_LEN=%u\r\n", (unsigned)dataset_tlvs_len);
    }

    ot_err = tcs_validate_dataset_tlvs(dataset_tlvs, dataset_tlvs_len);
    printf("TCS_PULL_STEP dataset_parse status=%s\r\n", otThreadErrorToString(ot_err));
    if (ot_err != OT_ERROR_NONE) {
        printf("TCS_PULL_END status=fail\r\n");
        goto exit;
    }

    if (args.mode == TCS_MODE_APPLY || args.mode == TCS_MODE_APPLY_START) {
        ot_err = tcs_apply_dataset_tlvs(dataset_tlvs, dataset_tlvs_len, args.mode == TCS_MODE_APPLY_START);
        printf("TCS_PULL_STEP dataset_apply status=%s\r\n", otThreadErrorToString(ot_err));
        if (ot_err != OT_ERROR_NONE) {
            printf("TCS_PULL_END status=fail\r\n");
            goto exit;
        }
    }

    printf("TCS_PULL_END status=ok\r\n");

exit:
    mbedtls_platform_zeroize(dataset_tlvs, sizeof(dataset_tlvs));
    mbedtls_platform_zeroize(&args, sizeof(args));
    tcs_task_mark_done();
    vTaskDelete(NULL);
}

static void tcs_scan_task(void *arg)
{
    tcs_scan_args_t args;
    mdns_result_t *ptr_results = NULL;
    uint32_t index = 0;
    TickType_t started_tick;
    uint32_t elapsed_ms = 0;
    const char *end_status = "ok";
    esp_err_t err;

    (void)arg;
    memcpy(&args, &sTcsScanArgs, sizeof(args));
    started_tick = xTaskGetTickCount();

    printf("TCS_MDNS_SCAN_BEGIN service=" TCS_MDNS_SERVICE "." TCS_MDNS_PROTO
           ".local timeout_ms=%u\r\n", (unsigned)args.timeout_ms);

    err = mdns_query_ptr(TCS_MDNS_SERVICE, TCS_MDNS_PROTO,
                         args.timeout_ms, TCS_MDNS_SCAN_MAX_RESULTS, &ptr_results);
    if (err != 0) {
        printf("TCS_MDNS_SCAN_ERROR query_ptr err=%d\r\n", err);
        end_status = "fail";
        goto exit;
    }

    for (mdns_result_t *ptr = ptr_results; ptr != NULL; ptr = ptr->next) {
        mdns_result_t *srv_results = NULL;
        const mdns_result_t *detail = ptr;
        const mdns_result_t *txt_detail = ptr;
        const char *instance = ptr->instance_name != NULL ? ptr->instance_name : "";
        uint32_t address_index = 0;
        uint32_t remaining_ms;

        if (ptr->port == 0 || ptr->hostname == NULL || ptr->addr == NULL) {
            remaining_ms = tcs_scan_remaining_ms(started_tick, args.timeout_ms);
            if (remaining_ms > 0) {
                err = mdns_query_srv(instance, TCS_MDNS_SERVICE, TCS_MDNS_PROTO, remaining_ms, &srv_results);
                if (err == 0 && srv_results != NULL) {
                    detail = srv_results;
                }
            }
        }

        if (detail->txt != NULL && detail->txt_count > 0) {
            txt_detail = detail;
        }

        tcs_print_mdns_summary(index, instance, detail, txt_detail);

        for (const mdns_ip_addr_t *addr = detail->addr; addr != NULL; addr = addr->next) {
            tcs_print_mdns_endpoint(index, address_index, addr, detail->nif, detail->port);
            address_index++;
        }
        if (address_index == 0) {
            printf("TCS_MDNS_ENDPOINT index=%u addr_index=none\r\n", (unsigned)index);
        }

        tcs_print_mdns_txt(index, txt_detail);

        if (srv_results != NULL) {
            mdns_query_results_free(srv_results);
        }

        index++;
    }

    elapsed_ms = (uint32_t)((xTaskGetTickCount() - started_tick) * portTICK_PERIOD_MS);
    if (index == 0 && elapsed_ms >= args.timeout_ms) {
        end_status = "timeout";
    }

exit:
    if (ptr_results != NULL) {
        mdns_query_results_free(ptr_results);
    }
    if (elapsed_ms == 0) {
        elapsed_ms = (uint32_t)((xTaskGetTickCount() - started_tick) * portTICK_PERIOD_MS);
    }
    printf("TCS_MDNS_SCAN_END status=%s count=%u elapsed_ms=%u\r\n",
           end_status, (unsigned)index, (unsigned)elapsed_ms);
    tcs_task_mark_done();
    vTaskDelete(NULL);
}

static void cmd_tcs_pull(char *buf, int len, int argc, char **argv)
{
    uint32_t port;
    tcs_mode_t mode;

    (void)buf;
    (void)len;

    if (argc == 2 && strcmp(argv[1], "help") == 0) {
        tcs_print_usage();
        return;
    }

    if (argc < 4 || argc > 5) {
        tcs_print_usage();
        return;
    }

    if (sTcsTaskHandle != NULL) {
        printf("TCS_PULL_ERROR busy\r\n");
        return;
    }

    if (strlen(argv[1]) == 0 || strlen(argv[1]) > TCS_CODE_MAX_LEN ||
        strlen(argv[2]) == 0 || strlen(argv[2]) > TCS_HOST_MAX_LEN) {
        printf("TCS_PULL_ERROR invalid_args\r\n");
        return;
    }

    if (!tcs_parse_uint32(argv[3], UINT16_MAX, &port) || port == 0) {
        printf("TCS_PULL_ERROR invalid_port\r\n");
        return;
    }

    if (tcs_parse_mode(argc == 5 ? argv[4] : NULL, &mode) != 0) {
        printf("TCS_PULL_ERROR invalid_mode\r\n");
        tcs_print_usage();
        return;
    }

    memset(&sTcsArgs, 0, sizeof(sTcsArgs));
    strncpy(sTcsArgs.code, argv[1], sizeof(sTcsArgs.code) - 1);
    strncpy(sTcsArgs.host, argv[2], sizeof(sTcsArgs.host) - 1);
    sTcsArgs.port = (uint16_t)port;
    sTcsArgs.mode = mode;

    if (xTaskCreate(tcs_pull_task, "tcs_pull", TCS_TASK_STACK_DEPTH, NULL, TCS_TASK_PRIORITY, &sTcsTaskHandle) != pdPASS) {
        sTcsTaskHandle = NULL;
        mbedtls_platform_zeroize(&sTcsArgs, sizeof(sTcsArgs));
        printf("TCS_PULL_ERROR task_create_failed\r\n");
    }
}

static void cmd_tcs_scan(char *buf, int len, int argc, char **argv)
{
    uint32_t timeout_ms = TCS_MDNS_SCAN_TIMEOUT_MS;

    (void)buf;
    (void)len;

    if (argc == 2 && strcmp(argv[1], "help") == 0) {
        tcs_print_scan_usage();
        return;
    }

    if (argc > 2) {
        tcs_print_scan_usage();
        return;
    }

    if (sTcsTaskHandle != NULL) {
        printf("TCS_MDNS_SCAN_ERROR busy\r\n");
        return;
    }

    if ((argc >= 2 && !tcs_parse_uint32(argv[1], UINT32_MAX, &timeout_ms)) || timeout_ms == 0) {
        printf("TCS_MDNS_SCAN_ERROR invalid_args\r\n");
        tcs_print_scan_usage();
        return;
    }

    memset(&sTcsScanArgs, 0, sizeof(sTcsScanArgs));
    sTcsScanArgs.timeout_ms = (uint32_t)timeout_ms;

    if (xTaskCreate(tcs_scan_task, "tcs_scan", TCS_TASK_STACK_DEPTH, NULL, TCS_TASK_PRIORITY, &sTcsTaskHandle) != pdPASS) {
        sTcsTaskHandle = NULL;
        memset(&sTcsScanArgs, 0, sizeof(sTcsScanArgs));
        printf("TCS_MDNS_SCAN_ERROR task_create_failed\r\n");
    }
}

const static struct cli_command tcs_cmds[] STATIC_CLI_CMD_ATTRIBUTE = {
    {"tcs_pull", "Pull Thread dataset TLVs using a Thread 1.4 one-time credential", cmd_tcs_pull},
    {"tcs_scan", "Scan Thread 1.4 credential-sharing Border Agents over mDNS", cmd_tcs_scan},
};

void tcs_credential_pull_init(void)
{
    /* Referenced from main.c so this object is pulled from the app archive. */
}
