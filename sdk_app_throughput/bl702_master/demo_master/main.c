#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <vfs.h>
#include <device/vfs_uart.h>
#include <aos/kernel.h>
#include <aos/yloop.h>
#include <event_device.h>
#include <cli.h>

#include <bl_sys.h>
#include <bl_chip.h>
#include <bl_wireless.h>
#include <bl_irq.h>
#include <bl_sec.h>
#include <bl_rtc.h>
#include <bl_uart.h>
#include <bl_gpio.h>
#include <bl_flash.h>
#include <bl_timer.h>
#include <bl_wdt.h>
#include <hal_boot2.h>
#include <hal_board.h>
#include <hosal_uart.h>
#include <hal_gpio.h>
#include <hal_pds.h>
#include <hal_tcal.h>

#include <easyflash.h>
#include <libfdt.h>
#include <utils_log.h>
#include <blog.h>

#include <lwip/netifapi.h>

#if defined(CFG_USE_PSRAM)
#include "bl_psram.h"
#endif /* CFG_USE_PSRAM */

HOSAL_UART_DEV_DECL(uart_stdio, 0, 14, 15, 2000000);

extern uint8_t _heap_start;
extern uint8_t _heap_size;  // @suppress("Type cannot be resolved")
extern uint8_t _heap2_start;
extern uint8_t _heap2_size;  // @suppress("Type cannot be resolved")
static HeapRegion_t xHeapRegions[] = {
    {&_heap_start, (unsigned int)&_heap_size},  // set on runtime
    {&_heap2_start, (unsigned int)&_heap2_size},
    {NULL, 0}, /* Terminates the array. */
    {NULL, 0}  /* Terminates the array. */
};
#if defined(CFG_USE_PSRAM)
extern uint8_t _heap3_start;
extern uint8_t _heap3_size; // @suppress("Type cannot be resolved")
static HeapRegion_t xHeapRegionsPsram[] =  
{
    { &_heap3_start, (size_t)(uintptr_t)&_heap3_size },
    { NULL, 0 }, /* Terminates the array. */
    { NULL, 0 } /* Terminates the array. */
};
#endif /* CFG_USE_PSRAM */

#define TIME_5MS_IN_32768CYCLE (164)  // (5000/(1000000/32768))
bool pds_start = false;

#if 0
/* event callback */
static int virt_net_spi_event_cb(virt_net_t obj, enum virt_net_event_code code,
                                 void *opaque)
{
    assert(obj != NULL);
    static int dhcp_started = 0;

    switch (code) {
        case VIRT_NET_EV_ON_CONNECTED:
            if (dhcp_started == 0) {
                dhcp_started = 1;
                netifapi_netif_set_up((struct netif *)&obj->netif);
                printf("start dhcp...\r\n");
                /* start dhcp */
                netifapi_dhcp_start((struct netif *)&obj->netif);
            }
            break;

        case VIRT_NET_EV_ON_DISCONNECT:
            if (dhcp_started == 1) {
                dhcp_started = 0;
                /* stop dhcp */
                printf("stop dhcp...\r\n");
                netifapi_dhcp_stop((struct netif *)&obj->netif);

                netifapi_netif_set_down((struct netif *)&obj->netif);
            }
            break;

        default:
            break;
    }

    return 0;
}

static void cmd_master_lwip(char *buf, int len, int argc, char **argv)
{
    tp_spi_config_t spi_cfg = {
        .port = 0,
        .spi_mode = 0,
        .spi_speed = 12000000,
        .miso = 4,
        .mosi = 5,
        .clk = 3,
        .cs = 6,
        .irq_pin = 10,
    };

    virt_net_t vnet_spi = virt_net_spi_create(&spi_cfg);
    if (vnet_spi == NULL) {
        printf("Create spi virtnet failed!!!!\r\n");
        return;
    }

    if (vnet_spi->init(vnet_spi)) {
        printf("init spi virtnet failed!!!!\r\n");
        return;
    }

    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gw;

    IP4_ADDR(&ipaddr, 192, 168, 31, 111);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gw, 192, 168, 31, 1);

    netifapi_netif_set_addr((struct netif *)&vnet_spi->netif, &ipaddr, &netmask,
                            &gw);

    virt_net_setup_callback(vnet_spi, virt_net_spi_event_cb, NULL);

    /* set to default netif */
    netifapi_netif_set_default(&vnet_spi->netif);

    if (argc > 2) {
        virt_net_connect_ap(vnet_spi, argv[1], argv[2]);
    } else {
        /* Connect AP */
        virt_net_connect_ap(vnet_spi, "ASUS_AX56U", "wangkun2115.");
    }

#warning "这个命令复位之后再执行."
}

const static struct cli_command cmds_user[] STATIC_CLI_CMD_ATTRIBUTE = {
    {"master", "master", cmd_master},
    {"wifi_connect", "master with lwip", cmd_master_lwip},
};
#endif

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    puts("Stack Overflow checked\r\n");
    if (pcTaskName) {
        printf("Stack name %s\r\n", pcTaskName);
    }
    while (1) {
        /*empty here*/
    }
}

void vApplicationMallocFailedHook(void)
{
    printf("Memory Allocate Failed. Current left size is %d bytes\r\n",
           xPortGetFreeHeapSize());
    while (1) {
        /*empty here*/
    }
}

void vApplicationIdleHook(void)
{
    __asm volatile("   wfi     ");
    /*empty*/
}

void vApplicationSleep(TickType_t xExpectedIdleTime) {}

#if (configUSE_TICK_HOOK != 0)
void vApplicationTickHook(void) {}
#endif

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    /* If the buffers to be provided to the Idle task are declared inside this
      function then they must be declared static - otherwise they will be
      allocated on the stack and so not exists after this function exits. */
    static StaticTask_t xIdleTaskTCB;
    // static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];
    static StackType_t uxIdleTaskStack[256];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task's
      state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task's stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
      Note that, as the array is necessarily of type StackType_t,
      configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    //*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
    *pulIdleTaskStackSize =
        256;  // size 256 words is For ble pds mode, otherwise stack overflow of
              // idle task will happen.
}

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so
the application must provide an implementation of
vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    /* If the buffers to be provided to the Timer task are declared inside this
      function then they must be declared static - otherwise they will be
      allocated on the stack and so not exists after this function exits. */
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
      task's state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task's stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
      Note that, as the array is necessarily of type StackType_t,
      configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void user_vAssertCalled(void) __attribute__((weak, alias("vAssertCalled")));
void vAssertCalled(void)
{
    volatile uint32_t ulSetTo1ToExitFunction = 0;

    taskDISABLE_INTERRUPTS();
    while (ulSetTo1ToExitFunction != 1) {
        __asm volatile("NOP");
    }
}

static void _cli_init()
{
    extern int network_netutils_iperf_cli_register();
    network_netutils_iperf_cli_register();

    // int lramsync_test_cli_init(void);
    // lramsync_test_cli_init();

    // int ramsync_test_cli_init(void);
    // ramsync_test_cli_init();

    int virt_net_test_cli_init(void);
    virt_net_test_cli_init();
}

static int get_dts_addr(const char *name, uint32_t *start, uint32_t *off)
{
    uint32_t addr = hal_board_get_factory_addr();
    const void *fdt = (const void *)addr;
    uint32_t offset;

    if (!name || !start || !off) {
        return -1;
    }

    offset = fdt_subnode_offset(fdt, 0, name);
    if (offset <= 0) {
        log_error("%s NULL.\r\n", name);
        return -1;
    }

    *start = (uint32_t)fdt;
    *off = offset;

    return 0;
}

static void aos_loop_proc(void *pvParameters)
{
    int fd_console;
    uint32_t fdt = 0, offset = 0;

    vfs_init();
    vfs_device_init();

    /* uart */
    const char *uart_node[] = {
        "uart@4000A000",
        "uart@4000A100",
    };

    if (0 == get_dts_addr("uart", &fdt, &offset)) {
        vfs_uart_init(fdt, offset, uart_node, 2);
    }

    aos_loop_init();

    fd_console = aos_open("/dev/ttyS0", 0);
    if (fd_console >= 0) {
        printf("Init CLI with event Driven\r\n");
        aos_cli_init(0);
        aos_poll_read_fd(fd_console, aos_cli_event_cb_read_get(),
                         (void *)0x12345678);
        _cli_init();
    }

    // cmd_master_lwip(NULL, NULL, NULL, NULL);
    aos_loop_run();

    puts("------------------------------------------\r\n");
    puts("+++++++++Critical Exit From Loop++++++++++\r\n");
    puts("******************************************\r\n");
    vTaskDelete(NULL);
}

#if 0
static void proc_hellow_entry(void *pvParameters)
{
    vTaskDelay(500);

    while (1) {
        printf("%s: RISC-V rv32imafc\r\n", __func__);
        vTaskDelay(10000);
    }
    vTaskDelete(NULL);
}
#endif

static void _dump_boot_info(void)
{
    char chip_feature[40];
    const char *banner;

    puts("Booting BL702 Chip...\r\n");

    /*Display Banner*/
    if (0 == bl_chip_banner(&banner)) {
        //        puts(banner);
    }
    puts("\r\n");
    /*Chip Feature list*/
    puts("\r\n");
    puts("------------------------------------------------------------\r\n");
    puts("RISC-V Core Feature:");
    bl_chip_info(chip_feature);
    puts(chip_feature);
    puts("\r\n");

    puts("Build Version: ");
    puts(BL_SDK_VER);  // @suppress("Symbol is not resolved")
    puts("\r\n");

    puts("Std BSP Driver Version: ");
    puts(BL_SDK_STDDRV_VER);  // @suppress("Symbol is not resolved")
    puts("\r\n");

    puts("Std BSP Common Version: ");
    puts(BL_SDK_STDCOM_VER);  // @suppress("Symbol is not resolved")
    puts("\r\n");

    puts("RF Version: ");
    puts(BL_SDK_RF_VER);  // @suppress("Symbol is not resolved")
    puts("\r\n");

    puts("Build Date: ");
    puts(__DATE__);
    puts("\r\n");
    puts("Build Time: ");
    puts(__TIME__);
    puts("\r\n");
    puts("------------------------------------------------------------\r\n");
}

static void system_init(void)
{
    blog_init();
    bl_irq_init();
    bl_rtc_init();
    hal_boot2_init();
    bl_sec_init();

    /* board config is set after system is init*/
    hal_board_cfg(0);
    hosal_dma_init();

#if defined(CFG_USE_PSRAM)
    bl_psram_init();
    vPortDefineHeapRegionsPsram(xHeapRegionsPsram);
#endif /*CFG_USE_PSRAM*/
}

static void system_thread_init()
{ /*nothing here*/
}

void setup_heap()
{
    bl_sys_em_config();

    // Invoked during system boot via start.S
    vPortDefineHeapRegions(xHeapRegions);
}

void bl702_main()
{
    static StackType_t aos_loop_proc_stack[1024];
    static StaticTask_t aos_loop_proc_task;
    // static StackType_t proc_hellow_stack[512];
    // static StaticTask_t proc_hellow_task;

    bl_sys_early_init();

    /*Init UART In the first place*/
    hosal_uart_init(&uart_stdio);
    puts("Starting bl702 now....\r\n");

    bl_sys_init();

    _dump_boot_info();

    printf(
        "Heap %u@%p, %u@%p"
        "\r\n",
        (unsigned int)&_heap_size, &_heap_start, (unsigned int)&_heap2_size,
        &_heap2_start);

    system_init();
    system_thread_init();

    // puts("[OS] Starting proc_hellow_entry task...\r\n");
    // xTaskCreateStatic(proc_hellow_entry, (char*)"hellow",
    // sizeof(proc_hellow_stack)/4, NULL, 15, proc_hellow_stack,
    // &proc_hellow_task);
    puts("[OS] Starting aos_loop_proc task...\r\n");
    xTaskCreateStatic(aos_loop_proc, (char *)"event_loop",
                      sizeof(aos_loop_proc_stack) / 4, NULL, 15,
                      aos_loop_proc_stack, &aos_loop_proc_task);

    puts("[OS] Starting TCP/IP Stack...\r\n");
    tcpip_init(NULL, NULL);

    puts("[OS] Starting OS Scheduler...\r\n");
    vTaskStartScheduler();
}
