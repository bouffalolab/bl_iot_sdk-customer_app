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
#include <bl702l.h>
#include <bl_flash.h>
#include <bl_timer.h>
#include <bl_irq.h>
#include <lmac154.h>
#include <lmac154_fpt.h>
#include <zb_timer.h>
#include <openthread_port.h>
#include <openthread/thread.h>
#include <openthread/thread_ftd.h>
#include <openthread/icmp6.h>
#include <openthread/cli.h>
#include <openthread/ncp.h>
#include <openthread/coap.h>

#ifdef SYS_AOS_CLI_ENABLE
void _cli_init(int fd_console)
{
    ot_uartSetFd(fd_console);
}
#endif

void otrInitUser(otInstance * instance)
{
#if CONFIG_OT_NCP || CONFIG_OT_RCP
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

#if OPENTHREAD_FTD
    lmac154_setFramePendingMode(LMAC154_FPT_ANY);
#endif

    zb_timer_cfg(bl_timer_now_us64() >> LMAC154_US_PER_SYMBOL_BITS);
    lmac154_disableRx();

    bl_irq_register(M154_IRQn, lmac154_getInterruptCallback());
    bl_irq_enable(M154_IRQn);
}

int main(int argc, char *argv[])
{
    bl_flash_init();

    lmac154_app_init();

    otrStart();

    return 0;
}
