#include <stdio.h>
#ifdef CFG_USE_FLASH_CODE
#include <bl_flash.h>
#endif
#include <openthread_port.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/icmp6.h>
#include <openthread/cli.h>
#include <openthread/ncp.h>
#include <openthread/coap.h>
#include <ot_utils_ext.h>

#ifdef SYS_AOS_CLI_ENABLE
void _cli_init(int fd_console)
{
    ot_uartSetFd(fd_console);
}
#endif

void otrInitUser(otInstance * instance)
{
#ifdef OT_NCP
    otAppNcpInit((otInstance * )instance);
#else
    otAppCliInit((otInstance * )instance);
#endif
}

void otrAppProcess(ot_system_event_t sevent) 
{
    /** for application code */
    /** Note,   NO heavy execution, no delay and semaphore pending here.
     *          do NOT stop/suspend this task */

}

int main(int argc, char *argv[])
{
    otRadio_opt_t opt;

#ifdef CFG_USE_FLASH_CODE
    bl_flash_init();
#endif

    ot_utils_init();

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

    otrStart(opt);

    return 0;
}
