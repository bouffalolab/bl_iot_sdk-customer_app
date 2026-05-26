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
#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>

#include <bl702.h>
#ifdef CONFIG_BLE_ENABLE
#include <bl702_glb.h>
#include <bl702_uart.h>
#endif

#include <bl_flash.h>
#include <bl_timer.h>
#include <bl_wireless.h>
#include <bl_irq.h>
#include <lmac154.h>
#include <lmac154_fpt.h>
#include <zb_timer.h>

#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/icmp6.h>
#include <openthread/cli.h>
#include <openthread/ncp.h>
#include <openthread/coap.h>
#include <openthread_port.h>

#ifdef CONFIG_BLE_ENABLE
#include <ble_lib_api.h>
#endif

void vApplicationTickHook( void )
{
#if defined(CFG_USB_CDC_ENABLE)
    extern void usb_cdc_monitor(void);
    usb_cdc_monitor();
#endif
}

void vApplicationSleep( TickType_t xExpectedIdleTime )
{
    
}

void otrAppProcess(ot_system_event_t sevent) 
{
    /** for application code */
    /** Note,   NO heavy execution, no delay and semaphore pending here.
     *          do NOT stop/suspend this task */

}

#ifdef SYS_AOS_CLI_ENABLE
void _cli_init(int fd_console)
{
#ifdef SYS_AOS_CLI_ENABLE
    ot_uartSetFd(fd_console);
#endif
#if defined(CFG_USB_CDC_ENABLE)
    extern void usb_cdc_start(int fd_console);
    usb_cdc_start(fd_console);
#endif
}
#endif

#if defined(CFG_USB_CDC_ENABLE)
void usb_cdc_update_serial_number(uint32_t * pdeviceserial0, uint32_t * pdeviceserial1, uint32_t * pdeviceserial2) 
{
    uint8_t addr[8] = {0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8};

    bl_wireless_mac_addr_get(addr);
 
    *pdeviceserial0 = 0x0 + ((uint32_t *)addr)[0];
    *pdeviceserial1 = 0x1 + ((uint32_t *)addr)[1];
    *pdeviceserial2 = 0x2;

    (*pdeviceserial0) += (*pdeviceserial2);
}
#endif

void otrInitUser(otInstance * instance)
{
#if CONFIG_OT_RCP || CONFIG_OT_NCP
    otAppNcpInit((otInstance * )instance);
#else
    otAppCliInit((otInstance * )instance);
#endif
}

#ifdef CONFIG_BLE_ENABLE
static void uart_gpio_init(void)
{
    GLB_GPIO_Cfg_Type cfg;

    // set GPIO as UART0 RX
    cfg.gpioPin  = 3;
    cfg.gpioFun  = GPIO3_FUN_UART_SIG3_UART_SIG7;

    cfg.gpioMode = GPIO_MODE_AF;
    cfg.pullType = GPIO_PULL_NONE;
    GLB_GPIO_Init(&cfg);

    // set GPIO as UART0 TX
    cfg.gpioPin  = 4;
    cfg.gpioFun  = GPIO4_FUN_UART_SIG4_UART_SIG0;

    cfg.gpioMode = GPIO_MODE_AF;
    cfg.pullType = GPIO_PULL_NONE;
    GLB_GPIO_Init(&cfg);

    // select UART GPIO function
    GLB_UART_Fun_Sel(GLB_UART_SIG_3,GLB_UART_SIG_FUN_UART1_RXD);
    GLB_UART_Fun_Sel(GLB_UART_SIG_4,GLB_UART_SIG_FUN_UART1_TXD);
}
#endif

static void lmac154_app_init(void)
{
    lmac154_init();
    lmac154_enableCoex();
    lmac154_setStd2015Extra(true);
    lmac154_setTxRetry(0);
    lmac154_fptClear();
    lmac154_setEnhAckWaitTime((LMAC154_AIFS + 10 + (6 + 42) * 2) << LMAC154_US_PER_SYMBOL_BITS);
    lmac154_setRxStateWhenIdle(true);

    lmac154_setTxRxTransTime(0xA0);

    zb_timer_cfg(bl_timer_now_us64() >> LMAC154_US_PER_SYMBOL_BITS);
    lmac154_disableRx();

    bl_irq_register(M154_IRQn, lmac154_getInterruptCallback());
    bl_irq_enable(M154_IRQn);
}

int main(int argc, char *argv[])
{
    lmac154_app_init();

    otrStart();

#ifdef CONFIG_BLE_ENABLE
    uart_gpio_init();
    extern void ble_uart_init(uint8_t uartid);
    ble_uart_init(1);

    ble_controller_init(configMAX_PRIORITIES - 1);
#endif

    return 0;
}
