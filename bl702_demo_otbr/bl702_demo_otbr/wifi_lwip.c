
#include <bl702_common.h>

#include <bl_timer.h>
#include <hosal_gpio.h>

#include <aos/yloop.h>

#include <virt_net.h>
#include <pkg_protocol.h>

#include <easyflash.h>
#ifdef CFG_BLE_ENABLE
#include <blsync_ble_app.h>
#endif
#include "main.h"

#define WIFI_INFO_EASYFLASH_KEY "blf-otbr-wifi-cred"
typedef struct __wifi_info {
    bool        is_wifi_info_saved;
    char        wifi_ssid[36];
    char        wifi_pwd[68];
} wifi_info_t;

static virt_net_t vnet_spi;
static wifi_info_t wifi_info;

#ifdef WIFI_LWIP_RESET_PIN
void wifi_lwip_hw_reset(void)
{
    hosal_gpio_dev_t gpio_led = {
        .config = OUTPUT_OPEN_DRAIN_NO_PULL,
        .priv = NULL
    };

    gpio_led.port = WIFI_LWIP_RESET_PIN;
    hosal_gpio_init(&gpio_led);
    
    hosal_gpio_output_set(&gpio_led, 0);
    vTaskDelay(100);

    hosal_gpio_output_set(&gpio_led, 1);
    vTaskDelay(300);
}
#endif

struct netif * otbr_getBackboneNetif(void) 
{
    return (struct netif*)&vnet_spi->netif;
}

/* event callback */
static int virt_net_spi_event_cb(virt_net_t obj, enum virt_net_event_code code,
                                 void *opaque)
{
    configASSERT(obj != NULL);
    static int dhcp_started = 0;
    ip6_addr_t* ip6addr;
    bool isIPv6AddressAssigend = false;

    switch (code) {
        case VIRT_NET_EV_ON_CONNECTED:
            printf("AP connect success!\r\n");
            if (dhcp_started == 0) {
                dhcp_started = 1;
            }
            break;
        case VIRT_NET_EV_ON_DISCONNECT:
            printf("AP disconnect !\r\n");
            break;
        case VIRT_NET_EV_ON_SCAN_DONE: {
#ifdef CFG_BLE_ENABLE
            netbus_wifi_mgmr_msg_cmd_t *pkg_data;
            netbus_fs_scan_ind_cmd_msg_t *msg;

            pkg_data = (netbus_wifi_mgmr_msg_cmd_t *)((struct pkg_protocol *)opaque)->payload;
            msg = (netbus_fs_scan_ind_cmd_msg_t*)((netbus_fs_scan_ind_cmd_msg_t*)pkg_data);

            blesync_wifi_scan_done(msg);
#endif

            break;
        }
        case VIRT_NET_EV_ON_LINK_STATUS_DONE:{
            struct bflbwifi_ap_record* record;
            netbus_fs_link_status_ind_cmd_msg_t* pkg_data;
        
            pkg_data = (netbus_fs_link_status_ind_cmd_msg_t *)((struct pkg_protocol *)opaque)->payload;
            record = &pkg_data->record;
            
            if (record->link_status == BF1B_WIFI_LINK_STATUS_UP) {
                printf("link status up!\r\n");
            } else if (record->link_status == BF1B_WIFI_LINK_STATUS_DOWN){
                printf("link status down!\r\n");
            } else {
                printf("link status unknown!\r\n");
            }

            printf("ssid:%s\r\n", record->ssid);
            printf("bssid: %02x%02x%02x%02x%02x%02x\r\n", 
                    record->bssid[0],
                    record->bssid[1],
                    record->bssid[2],
                    record->bssid[3],
                    record->bssid[4],
                    record->bssid[5]);
            break;
        }
        case VIRT_NET_EV_ON_GOT_IP: {
            printf("[lwip] netif status callback\r\n");
#if LWIP_IPV4
            printf("IP: %s\r\n", ip4addr_ntoa(netif_ip4_addr(&obj->netif)));
            printf("MASK: %s\r\n", ip4addr_ntoa(netif_ip4_netmask(&obj->netif)));
            printf("Gateway: %s\r\n", ip4addr_ntoa(netif_ip4_gw(&obj->netif)));
#endif

            for (uint32_t i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i ++ ) {
                if (!ip6_addr_isany(netif_ip6_addr(&obj->netif, i))
                    && ip6_addr_ispreferred(netif_ip6_addr_state(&obj->netif, i))) {

                    ip6addr = (ip6_addr_t *)netif_ip6_addr(&obj->netif, i);
                    if (ip6_addr_isany(ip6addr)) {
                        continue;
                    }

                    if(ip6_addr_islinklocal(ip6addr)){
                        printf("LOCAL IP6 addr %s\r\n", ip6addr_ntoa(ip6addr));
                    }
                    else{
                        printf("GLOBAL IP6 addr %s\r\n", ip6addr_ntoa(ip6addr));
                        isIPv6AddressAssigend = true;
                    }
                }
            }

            if (isIPv6AddressAssigend) {
                otbr_instance_routing_init();

                if (!wifi_info.is_wifi_info_saved) {
                    ef_set_env_blob(WIFI_INFO_EASYFLASH_KEY, (void*)&wifi_info, sizeof(wifi_info));
                    wifi_info.is_wifi_info_saved = true;
                }
#ifdef CFG_BLE_ENABLE
                blsync_ble_stop();
#endif
            }

			break;
        }
        default:
            break;
    }

    return 0;
}

void cmd_connect(char *buf, int len, int argc, char **argv)
{
    if (argc > 2) {
        virt_net_connect_ap(vnet_spi, argv[1], argv[2]);

        if (strlen(argv[1]) < sizeof(wifi_info.wifi_ssid) && 
            strlen(argv[2]) < sizeof(wifi_info.wifi_pwd)) {

            strcpy(wifi_info.wifi_ssid, argv[1]);
            strcpy(wifi_info.wifi_pwd, argv[2]);
        }
    }
}

void cmd_disconnect(char *buf, int len, int argc, char **argv)
{
    virt_net_disconnect(vnet_spi);
}

void cmd_scan(char *buf, int len, int argc, char **argv)
{
    virt_net_scan(vnet_spi);
}

void cmd_get_info(char *buf, int len, int argc, char **argv)
{
    virt_net_get_link_status(vnet_spi);
}

void wifi_lwip_init(void)
{
    size_t saved_value_len = 0;

    vnet_spi = virt_net_create(NULL);
    if (vnet_spi == NULL) {
        printf("Create vnet_net virtnet failed!!!!\r\n");
        return;
    }

    if (vnet_spi->init(vnet_spi)) {
        printf("init spi virtnet failed!!!!\r\n");
        return;
    }

    virt_net_setup_callback(vnet_spi, virt_net_spi_event_cb, NULL);

    /* set to default netif */
    netifapi_netif_set_default((struct netif *)&vnet_spi->netif);

    ef_get_env_blob(WIFI_INFO_EASYFLASH_KEY, (void*)&wifi_info, sizeof(wifi_info), &saved_value_len);
    if (sizeof(wifi_info) == saved_value_len) {

        if (strlen(wifi_info.wifi_ssid) > 0 && strlen(wifi_info.wifi_ssid) < sizeof(wifi_info.wifi_ssid) &&
            strlen(wifi_info.wifi_pwd) >= 8 && strlen(wifi_info.wifi_pwd) < sizeof(wifi_info.wifi_ssid)) {

            printf("Connect to previous Wi-Fi network SSID %s, password %s.\r\n", wifi_info.wifi_ssid, wifi_info.wifi_pwd);
            wifi_info.is_wifi_info_saved = true;
            virt_net_connect_ap(vnet_spi, wifi_info.wifi_ssid, wifi_info.wifi_pwd);
            return;
        }
    }

#ifdef CFG_BLE_ENABLE
    blsync_ble_start();
#endif
    memset(&wifi_info, 0, sizeof(wifi_info));
}
