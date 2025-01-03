#include <bl_rtc.h>
#include <hal_tcal.h>

#include <cli.h>

#include <lwip/tcpip.h>

#include <openthread/dataset_ftd.h>
#if OPENTHREAD_CONFIG_BORDER_ROUTING_ENABLE
#include <openthread/border_router.h>
#endif /* OPENTHREAD_CONFIG_BORDER_ROUTING_ENABLE */
#if OPENTHREAD_CONFIG_BACKBONE_ROUTER_ENABLE
#include <openthread/backbone_router_ftd.h>
#endif /* OPENTHREAD_CONFIG_BORDER_ROUTING_ENABLE */
#include <openthread/platform/settings.h>
#include <openthread_port.h>

#include <main.h>

void vApplicationMallocFailedHook(void)
{
#if defined(CFG_USE_PSRAM)
    printf("Memory Allocate Failed. SRAM left: %d bytes, PSRAM left: %d bytes\r\n", 
            xPortGetFreeHeapSize(), xPortGetFreeHeapSizePsram());
#else
    printf("Memory Allocate Failed. SRAM left: %d bytes\r\n", xPortGetFreeHeapSize());
#endif

    while (1) {
        /*empty here*/
    }
}

#if defined(CFG_USB_CDC_ENABLE)
void vApplicationTickHook( void )
{
    extern void usb_cdc_monitor(void);
    usb_cdc_monitor();
}
#endif

void _cli_init(int fd_console)
{
#ifdef SYS_AOS_CLI_ENABLE
    ot_uartSetFd(fd_console);
#endif
#if defined(CFG_USB_CDC_ENABLE)
    extern void usb_cdc_start(int fd_console);
    usb_cdc_start(fd_console);
#endif /* CFG_USB_CDC_ENABLE */
}

static void cmd_ifconfig(char *buf, int len, int argc, char **argv)
{
    struct netif  * netif = otbr_getInfraNetif();
    ip6_addr_t    * ip6addr;
    
    printf("Infra net interface: %s\r\n", netif->flags & NETIF_FLAG_UP ? "UP": "DOWN");
    printf("\tMAC address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", netif->hwaddr[0], 
        netif->hwaddr[1], netif->hwaddr[2], netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);

    if (netif->flags & NETIF_FLAG_UP) {
        if(!ip4_addr_isany(netif_ip4_addr(netif))){
            printf("\tIPv4 address: %s\r\n", ip4addr_ntoa(netif_ip4_addr(netif)));
            printf("\tIPv4 mask: %s\r\n", ip4addr_ntoa(netif_ip4_netmask(netif)));
            printf("\tGateway address: %s\r\n", ip4addr_ntoa(netif_ip4_gw(netif)));
        }

        for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i ++ ) {
            if (!ip6_addr_isany(netif_ip6_addr(netif, i)) && 
                ip6_addr_ispreferred(netif_ip6_addr_state(netif, i))) {

                ip6addr = (ip6_addr_t *)netif_ip6_addr(netif, i);
                if (ip6_addr_isany(ip6addr)) {
                    continue;
                }

                if (ip6_addr_islinklocal(ip6addr)) {
                    printf("\tIPv6 linklocal address: %s\r\n", ip6addr_ntoa(ip6addr));
                }
                else{
                    printf("\tIPv6 address %d: %s\r\n", i, ip6addr_ntoa(ip6addr));
                }
            }
        }
    }

    netif = otbr_getThreadNetif();
    printf("Thread net interface: %s\r\n", netif->flags & NETIF_FLAG_UP ? "UP": "DOWN");
    printf("\tMAC address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", netif->hwaddr[0], 
        netif->hwaddr[1], netif->hwaddr[2], netif->hwaddr[3], netif->hwaddr[4], netif->hwaddr[5]);

    if (netif->flags & NETIF_FLAG_UP) {
        if(!ip4_addr_isany(netif_ip4_addr(netif))){
            printf("\tIPv4 address: %s\r\n", ip4addr_ntoa(netif_ip4_addr(netif)));
            printf("\tIPv4 mask: %s\r\n", ip4addr_ntoa(netif_ip4_netmask(netif)));
            printf("\tGateway address: %s\r\n", ip4addr_ntoa(netif_ip4_gw(netif)));
        }

        for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i ++ ) {
            if (!ip6_addr_isany(netif_ip6_addr(netif, i)) && 
                ip6_addr_ispreferred(netif_ip6_addr_state(netif, i))) {

                ip6addr = (ip6_addr_t *)netif_ip6_addr(netif, i);
                if (ip6_addr_isany(ip6addr)) {
                    continue;
                }

                if (ip6_addr_islinklocal(ip6addr)) {
                    printf("\tIPv6 linklocal address: %s\r\n", ip6addr_ntoa(ip6addr));
                }
                else{
                    printf("\tIPv6 address %d: %s\r\n", i, ip6addr_ntoa(ip6addr));
                }
            }
        }
    }
}

static void cmd_ipaddr(char *buf, int len, int argc, char **argv)
{
    bool isAdd = false;
    int index = 0;
    ip6_addr_t ip6addr;

    if (argc < 4) {
        return;
    }

    if (0 == strcmp(argv[1], "add")) {
        isAdd = true;
    }
    else if (0 == strcmp(argv[1], "del")) {
        isAdd = false;
    }
    else {
        return;
    }

    index = atoi(argv[2]);
    if (index >= LWIP_IPV6_NUM_ADDRESSES) {
        return;
    }

    if (!isAdd) {
        if (index) {
            netif_ip6_addr_set_state(otbr_getInfraNetif(), index, IP6_ADDR_INVALID);
        }
        return;
    }

    if (ip6addr_aton(argv[3], &ip6addr)) {
        netif_ip6_addr_set(otbr_getInfraNetif(), index, &ip6addr);
        netif_ip6_addr_set_state(otbr_getInfraNetif(), index, IP6_ADDR_PREFERRED);

        otbr_instance_routing_init();
    }
}

const static struct cli_command cmds_user[] STATIC_CLI_CMD_ATTRIBUTE = {
#ifdef CFG_USE_WIFI_BR
    {"wifi_sta_connect", "connect to Wi-Fi AP", cmd_connect},
    {"wifi_sta_disconnect", "wifi disconnect from Wi-Fi AP", cmd_disconnect},
    {"wifi_sta_scan", "wifi scan", cmd_scan},
    {"get_net_info", "get_net_info", cmd_get_info},
    {"wifi_config", "config wifi SSID & Password", cmd_wifi_config},
#endif /* CFG_USE_WIFI_BR */
    {"ifconfig", "ip addresses", cmd_ifconfig},
    {"ipaddr", "ipaddr operation", cmd_ipaddr},
};

void otrAppProcess(ot_system_event_t sevent) 
{
    /** for application code */
    /** Note,   NO heavy execution, no delay and semaphore pending here.
     *          do NOT stop/suspend this task */

}

#ifdef CFG_THREAD_AUTO_START
void otr_start_default(void) 
{
    otOperationalDataset ds;
    uint8_t default_network_key[] = THREAD_NETWORK_KEY;
    uint8_t default_extend_panid[] = THREAD_EXTPANID;

    if (!otDatasetIsCommissioned(otrGetInstance())) {

        if (OT_ERROR_NONE != otDatasetCreateNewNetwork(otrGetInstance(), &ds)) {
            printf("Failed to create dataset for Thread Network\r\n");
        }

        memcpy(&ds.mNetworkKey, default_network_key, sizeof(default_network_key));
        strncpy(ds.mNetworkName.m8, "OTBR-BL702", sizeof(ds.mNetworkName.m8));
        memcpy(&ds.mExtendedPanId, default_extend_panid, sizeof(default_extend_panid));
        ds.mChannel = THREAD_CHANNEL;
        ds.mPanId = THREAD_PANID;
        
        if (OT_ERROR_NONE != otDatasetSetActive(otrGetInstance(), &ds)) {
            printf("Failed to set active dataset\r\n");
        }
    }

    otIp6SetEnabled(otrGetInstance(), true);
    otThreadSetEnabled(otrGetInstance(), true);
}
#endif

void otrInitUser(otInstance * instance)
{
    otAppCliInit((otInstance * )instance);

#ifdef CFG_THREAD_AUTO_START
    otr_start_default();
#endif
    
    otbr_netif_init();
}

int main(int argc, char *argv[])
{
    otRadio_opt_t opt;

    bl_rtc_init();
    hal_tcal_init();

#if defined(CFG_USE_WIFI_BR)
    wifi_lwip_hw_reset();
#endif /* CFG_USE_WIFI_BR */

    otPlatSettingsInit(NULL, NULL, 0);

    opt.byte = 0;

#if OPENTHREAD_FTD
    opt.bf.isFtd = true;
#endif
#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
    opt.bf.isLinkMetricEnable = true;
#endif
#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE
    opt.bf.isCSLReceiverEnable = true;
#endif
#if OPENTHREAD_CONFIG_TIME_SYNC_ENABLE
    opt.bf.isTimeSyncEnable = true;
#endif

    tcpip_init(NULL, NULL);

#ifdef CFG_USE_WIFI_BR
    wifi_lwip_init();
#else
    eth_lwip_init();
#endif /*CFG_ETHERNET_ENABLE*/
    
    otrStart(opt);

    return 0;
}
