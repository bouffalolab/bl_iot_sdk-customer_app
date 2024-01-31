#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <vfs.h>
#include <aos/kernel.h>
#include <aos/yloop.h>
#include <event_device.h>
#include <cli.h>

#include <lwip/tcpip.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/tcp.h>
#include <lwip/err.h>
#include <http_client.h>
#include <netutils/netutils.h>

#include <bl602_glb.h>
#include <bl602_hbn.h>

#include <bl_uart.h>
#include <bl_chip.h>
#include <bl_wifi.h>
#include <hal_wifi.h>
#include <bl_sec.h>
#include <bl_cks.h>
#include <bl_irq.h>
#include <bl_dma.h>
#include <bl_timer.h>
#include <bl_gpio_cli.h>
#include <bl_wdt_cli.h>
#include <hosal_uart.h>
#include <hosal_adc.h>
#include <hal_sys.h>
#include <hal_gpio.h>
#include <hal_boot2.h>
#include <hal_board.h>
#include <bl_sys_time.h>
#include <bl_sys.h>
#include <bl_romfs.h>
#include <fdt.h>
#include <device/vfs_uart.h>
#include <easyflash.h>
#include <bl60x_fw_api.h>
#include <wifi_mgmr_ext.h>
#include <utils_log.h>
#include <libfdt.h>
#include <blog.h>
#include <bl_wps.h>

#ifdef CFG_NETBUS_WIFI_ENABLE
#include "netbus_mgmr.h"
#include "netbus_transceiver.h"
#endif

#define mainHELLO_TASK_PRIORITY     ( 20 )
#define UART_ID_2 (2)
#define WIFI_AP_PSM_INFO_SSID           "conf_ap_ssid"
#define WIFI_AP_PSM_INFO_PASSWORD       "conf_ap_psk"
#define WIFI_AP_PSM_INFO_PMK            "conf_ap_pmk"
#define WIFI_AP_PSM_INFO_BSSID          "conf_ap_bssid"
#define WIFI_AP_PSM_INFO_CHANNEL        "conf_ap_channel"
#define WIFI_AP_PSM_INFO_IP             "conf_ap_ip"
#define WIFI_AP_PSM_INFO_MASK           "conf_ap_mask"
#define WIFI_AP_PSM_INFO_GW             "conf_ap_gw"
#define WIFI_AP_PSM_INFO_DNS1           "conf_ap_dns1"
#define WIFI_AP_PSM_INFO_DNS2           "conf_ap_dns2"
#define WIFI_AP_PSM_INFO_IP_LEASE_TIME  "conf_ap_ip_lease_time"
#define WIFI_AP_PSM_INFO_GW_MAC         "conf_ap_gw_mac"
#define CLI_CMD_AUTOSTART1              "cmd_auto1"
#define CLI_CMD_AUTOSTART2              "cmd_auto2"


extern void demo_hbn_set_magic(void);
extern bool demo_hbn_is_wakup_from_hbn(void);
extern void ble_stack_start(void);
/* TODO: const */
volatile uint32_t uxTopUsedPriority __attribute__((used)) =  configMAX_PRIORITIES - 1;

static wifi_conf_t conf =
{
    .country_code = "CN",
};

static wifi_interface_t wifi_interface;

static unsigned char char_to_hex(char asccode)
{
    unsigned char ret;

    if('0'<=asccode && asccode<='9')
        ret=asccode-'0';
    else if('a'<=asccode && asccode<='f')
        ret=asccode-'a'+10;
    else if('A'<=asccode && asccode<='F')
        ret=asccode-'A'+10;
    else
        ret=0;

    return ret;
}

int check_dts_config(char ssid[33], char password[64])
{
    bl_wifi_ap_info_t sta_info;

    if (bl_wifi_sta_info_get(&sta_info)) {
        /*no valid sta info is got*/
        return -1;
    }

    strncpy(ssid, (const char*)sta_info.ssid, 32);
    ssid[31] = '\0';
    strncpy(password, (const char*)sta_info.psk, 64);
    password[63] = '\0';

    return 0;
}

static void wifi_sta_connect(char *ssid, char *password)
{
    wifi_interface_t wifi_interface;

    wifi_interface = wifi_mgmr_sta_enable();
    wifi_mgmr_sta_connect(wifi_interface, ssid, password, NULL, NULL, 0, 0);
}

static void send_ready_ind()
{
    netbus_slave_start_ind_msg_t msg;

    msg.hdr.cmd = BFLB_CMD_SLAVE_READY_IND;
    msg.hdr.msg_id = BFLB_CMD_SLAVE_READY_IND;
    msg.args.reserved = 0xFF;

    printf("send slave ready indication\r\n");

    bflbmsg_send(&g_netbus_wifi_mgmr_env.trcver_ctx, BF1B_MSG_TYPE_CMD, &msg, sizeof(msg));
}

static void event_cb_wifi_event(input_event_t *event, void *private_data)
{
#ifdef CFG_NETBUS_WIFI_ENABLE
	netbus_wifi_mgmr_msg_t swm_msg;
#endif
    static char *ssid;
    static char *password;

    switch (event->code) {
        case CODE_WIFI_ON_INIT_DONE:
        {
            printf("[APP] [EVT] INIT DONE %lld\r\n", aos_now_ms());
            wifi_mgmr_start_background(&conf);
        }
        break;
        case CODE_WIFI_ON_MGMR_DONE:
        {
            printf("[APP] [EVT] MGMR DONE %lld, now %lums\r\n", aos_now_ms(), bl_timer_now_us()/1000);
#ifdef CFG_NETBUS_WIFI_ENABLE
            netbus_wifi_mgmr_start(&g_netbus_wifi_mgmr_env);
            send_ready_ind();
#endif
        }
        break;
        case CODE_WIFI_ON_MGMR_DENOISE:
        {
            printf("[APP] [EVT] Microwave Denoise is ON %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_SCAN_DONE:
        {
            printf("[APP] [EVT] SCAN Done %lld\r\n", aos_now_ms());
            wifi_mgmr_cli_scanlist();
        }
        break;
        case CODE_WIFI_ON_SCAN_DONE_ONJOIN:
        {
            printf("[APP] [EVT] SCAN On Join %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_DISCONNECT:
        {
            printf("[APP] [EVT] disconnect %lld, Reason: %s\r\n",
                aos_now_ms(),
                wifi_mgmr_status_code_str(event->value)
            );
#ifdef CFG_NETBUS_WIFI_ENABLE
            memset(&swm_msg, 0, sizeof(swm_msg));
            swm_msg.type = NETBUS_WIFI_MGMR_MSG_TYPE_CMD;
            swm_msg.u.cmd.cmd = BFLB_CMD_AP_DISCONNECTED_IND;

            netbus_wifi_mgmr_msg_send(&g_netbus_wifi_mgmr_env, &swm_msg, true, false);
#endif
        }
        break;
        case CODE_WIFI_ON_CONNECTING:
        {
            printf("[APP] [EVT] Connecting %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_CMD_RECONNECT:
        {
            printf("[APP] [EVT] Reconnect %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_CONNECTED:
        {
            printf("[APP] [EVT] connected %lld\r\n", aos_now_ms());
#ifdef CFG_NETBUS_WIFI_ENABLE
            memset(&swm_msg, 0, sizeof(swm_msg));
            swm_msg.type = NETBUS_WIFI_MGMR_MSG_TYPE_CMD;
            swm_msg.u.cmd.cmd = BFLB_CMD_AP_CONNECTED_IND;

            netbus_wifi_mgmr_msg_send(&g_netbus_wifi_mgmr_env, &swm_msg, true, false);
#endif
        }
        break;
        case CODE_WIFI_ON_PRE_GOT_IP:
        {
            printf("[APP] [EVT] connected %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_GOT_IP:
        {
            printf("[APP] [EVT] GOT IP %lld\r\n", aos_now_ms());
            printf("[SYS] Memory left is %d Bytes\r\n", xPortGetFreeHeapSize());
        }
        break;
        case CODE_WIFI_ON_EMERGENCY_MAC:
        {
            printf("[APP] [EVT] EMERGENCY MAC %lld\r\n", aos_now_ms());
            hal_reboot();//one way of handling emergency is reboot. Maybe we should also consider solutions
        }
        break;
        case CODE_WIFI_ON_PROV_SSID:
        {
            printf("[APP] [EVT] [PROV] [SSID] %lld: %s\r\n",
                    aos_now_ms(),
                    event->value ? (const char*)event->value : "UNKNOWN"
            );
            if (ssid) {
                vPortFree(ssid);
                ssid = NULL;
            }
            ssid = (char*)event->value;
        }
        break;
        case CODE_WIFI_ON_PROV_BSSID:
        {
            printf("[APP] [EVT] [PROV] [BSSID] %lld: %s\r\n",
                    aos_now_ms(),
                    event->value ? (const char*)event->value : "UNKNOWN"
            );
            if (event->value) {
                vPortFree((void*)event->value);
            }
        }
        break;
        case CODE_WIFI_ON_PROV_PASSWD:
        {
            printf("[APP] [EVT] [PROV] [PASSWD] %lld: %s\r\n", aos_now_ms(),
                    event->value ? (const char*)event->value : "UNKNOWN"
            );
            if (password) {
                vPortFree(password);
                password = NULL;
            }
            password = (char*)event->value;
        }
        break;
        case CODE_WIFI_ON_PROV_CONNECT:
        {
            printf("[APP] [EVT] [PROV] [CONNECT] %lld\r\n", aos_now_ms());
            printf("connecting to %s:%s...\r\n", ssid, password);
            wifi_sta_connect(ssid, password);
        }
        break;
        case CODE_WIFI_ON_PROV_DISCONNECT:
        {
            printf("[APP] [EVT] [PROV] [DISCONNECT] %lld\r\n", aos_now_ms());
        }
        break;
        case CODE_WIFI_ON_AP_STA_ADD:
        {
            printf("[APP] [EVT] [AP] [ADD] %lld, sta idx is %lu\r\n", aos_now_ms(), (uint32_t)event->value);
        }
        break;
        case CODE_WIFI_ON_AP_STA_DEL:
        {
            printf("[APP] [EVT] [AP] [DEL] %lld, sta idx is %lu\r\n", aos_now_ms(), (uint32_t)event->value);
        }
        break;
        default:
        {
            printf("[APP] [EVT] Unknown code %u, %lld\r\n", event->code, aos_now_ms());
            /*nothing*/
        }
    }
}

static void send_heartbeat(TimerHandle_t xTimer)
{
    netbus_slave_heartbeat_msg_t msg;

    msg.hdr.cmd = BFLB_CMD_SLAVE_HEARTBEAT;
    msg.hdr.msg_id = BFLB_CMD_SLAVE_HEARTBEAT;
    msg.args.reserved = 0xFF;

    printf("send heartbeat\r\n");

    bflbmsg_send(&g_netbus_wifi_mgmr_env.trcver_ctx, BF1B_MSG_TYPE_CMD, &msg, sizeof(msg));
}

TimerHandle_t heartbeatTimerHdl = NULL;
#define SLAVE_HEARTBEAT_INTVL_IN_SEC 5
static void app_startHeartbeat()
{
    printf("start heartbeat\r\n");
    
    if(heartbeatTimerHdl && xTimerIsTimerActive(heartbeatTimerHdl))
    {
        return;
    }

    heartbeatTimerHdl = xTimerCreate("Heartbeat", pdMS_TO_TICKS(SLAVE_HEARTBEAT_INTVL_IN_SEC * 1000), 1, NULL,  send_heartbeat);
    if (heartbeatTimerHdl)
    {
        xTimerStart(heartbeatTimerHdl, 0);
    }
    else
    {
        printf("Failed to create heatbeat timer\r\n");
    }
}

static void _cli_init()
{
    /*Put CLI which needs to be init here*/
    easyflash_cli_init();
    network_netutils_iperf_cli_register();
    network_netutils_tcpserver_cli_register();
    network_netutils_tcpclinet_cli_register();
    network_netutils_netstat_cli_register();
    network_netutils_ping_cli_register();
    wifi_mgmr_cli_init();

    // int lramsync_test_cli_init(void);
    // lramsync_test_cli_init();

    int ramsync_test_cli_init(void);
    ramsync_test_cli_init();
    int bl602_hbn_cli_init(void);
    bl602_hbn_cli_init();
}

static void cmd_stack_wifi(char *buf, int len, int argc, char **argv)
{
    /*wifi fw stack and thread stuff*/
    static uint8_t stack_wifi_init  = 0;

    if (1 == stack_wifi_init) {
        puts("Wi-Fi Stack Started already!!!\r\n");
        return;
    }
    stack_wifi_init = 1;

    printf("Start Wi-Fi fw @%lums\r\n", bl_timer_now_us()/1000);
    hal_wifi_start_firmware_task();
    /*Trigger to start Wi-Fi*/
    printf("Start Wi-Fi fw is Done @%lums\r\n", bl_timer_now_us()/1000);
    aos_post_event(EV_WIFI, CODE_WIFI_ON_INIT_DONE, 0);
}

static void proc_main_entry(void *pvParameters)
{
    easyflash_init();

    _cli_init();

    aos_register_event_filter(EV_WIFI, event_cb_wifi_event, NULL);
    cmd_stack_wifi(NULL, 0, 0, NULL);
    app_startHeartbeat();

    vTaskDelete(NULL);
}

static void system_thread_init()
{
    /*nothing here*/
}


void app_handle_hbn(void *ptr, uint32_t length) 
{
    printf("bl602 enther hbn mode with wakup gpio 7 & 8\r\n");

    demo_hbn_set_magic();

    extern void demo_hbn_gpio(void);
    demo_hbn_gpio();
}

void main()
{
    bl_sys_init();

    system_thread_init();

#ifdef CONF_ENABLE_HBN
    if (!demo_hbn_is_wakup_from_hbn()) {
        app_handle_hbn(NULL, 0);
    }
#endif

    puts("[OS] Starting proc_mian_entry task...\r\n");
    xTaskCreate(proc_main_entry, (char*)"main_entry", 1024, NULL, 15, NULL);
    puts("[OS] Starting TCP/IP Stack...\r\n");
    tcpip_init(NULL, NULL);
}
