/*
 * Copyright (c) 2020 Bouffalolab.
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

#include "string.h"
#include "bl602_acomp.h"
#include "bl602_common.h"
#include "bl602_glb.h"
#include "bl602_hbn.h"
#include "bl602_gpio.h"
#include "bl602_xip_sflash.h"
#include "bl_irq.h"
#include "cli.h"
#include "bl_flash.h"

#define ACOMP1_GPIO_PIN        GLB_GPIO_PIN_12
#define ACOMP1_GPIO_FUN        GPIO12_FUN_GPIP_CH0_GPADC_VREF_EXT

#define CONF_ENABLE_HBN_MAGIC ('b' << 24 | 'l' << 16 | '6' << 8 | '0')
#define CONF_HBN_MAGIC_ADDR   (volatile uint32_t *)(HBN_RAM_BASE + 4)


static void ACOMP_GPIO_Init(void)
{
    GLB_GPIO_Cfg_Type cfg;

    cfg.gpioMode=GPIO_MODE_AF;
    cfg.pullType=GPIO_PULL_NONE;
    cfg.drive=1;
    cfg.smtCtrl=1;
    cfg.gpioPin=ACOMP1_GPIO_PIN;
    cfg.gpioFun=ACOMP1_GPIO_FUN;

    GLB_GPIO_Init(&cfg);
}

static void HBN_ACOMP01_IRQHandler_Cb(void)
{
    //acomp0IntCnt++;
    //HBN_Clear_IRQ(HBN_INT_ACOMP0);
    HBN_Clear_IRQ(HBN_INT_ACOMP1);
    printf("------------------------test\r\n");
    printf("acomp0 normal status is %d, acomp1 normal status is %d\r\n",AON_ACOMP_Get_Result(AON_ACOMP0_ID),AON_ACOMP_Get_Result(AON_ACOMP1_ID));
}

//void demo_hbn_acomp(uint32_t *time)
void demo_hbn_acomp()
{
    uint32_t vallow, valhig;
#if 1
    AON_ACOMP_CFG_Type acompCfg0={
        .muxEn=ENABLE,                                          /*!< ACOMP mux enable */
        .posChanSel=AON_ACOMP_CHAN_ADC4,                        /*!< ACOMP positive channel select */
        .negChanSel=AON_ACOMP_CHAN_VREF_1P2V,                   /*!< ACOMP negtive channel select */
        .levelFactor=AON_ACOMP_LEVEL_FACTOR_1,                  /*!< ACOMP level select factor */
        .biasProg=AON_ACOMP_BIAS_POWER_MODE1,                   /*!< ACOMP bias current control */
        .hysteresisPosVolt=AON_ACOMP_HYSTERESIS_VOLT_10MV,      /*!< ACOMP hysteresis voltage for positive */
        .hysteresisNegVolt=AON_ACOMP_HYSTERESIS_VOLT_10MV,      /*!< ACOMP hysteresis voltage for negtive */
    };
#endif
    AON_ACOMP_CFG_Type acompCfg1={
        .muxEn=ENABLE,                                          /*!< ACOMP mux enable */
        .posChanSel=AON_ACOMP_CHAN_ADC0,                        /*!< ACOMP positive channel select */
        .negChanSel=AON_ACOMP_CHAN_0P25VBAT,                   /*!< ACOMP negtive channel select */
        .levelFactor=AON_ACOMP_LEVEL_FACTOR_1,                  /*!< ACOMP level select factor */
        .biasProg=AON_ACOMP_BIAS_POWER_MODE1,                   /*!< ACOMP bias current control */
        .hysteresisPosVolt=AON_ACOMP_HYSTERESIS_VOLT_10MV,      /*!< ACOMP hysteresis voltage for positive */
        .hysteresisNegVolt=AON_ACOMP_HYSTERESIS_VOLT_10MV,      /*!< ACOMP hysteresis voltage for negtive */
    };
    
    SPI_Flash_Cfg_Type *flash_cfg;
    flash_cfg = bl_flash_get_flashCfg();
    
    HBN_APP_CFG_Type cfg={
        .useXtal32k=0,                                        /*!< Wheather use xtal 32K as 32K clock source,otherwise use rc32k */
        .sleepTime= 0,                                         /*!< HBN sleep time */
        .gpioWakeupSrc=HBN_WAKEUP_GPIO_NONE,                   /*!< GPIO Wakeup source */
        .gpioTrigType=HBN_GPIO_INT_TRIGGER_SYNC_FALLING_EDGE, /*!< GPIO Triger type */
        .flashCfg=flash_cfg,                                       /*!< Flash config pointer, used when power down flash */
        .hbnLevel=HBN_LEVEL_0,                                /*!< HBN level */
        .ldoLevel=HBN_LDO_LEVEL_1P10V,                        /*!< LDO level */
    };

    printf("HBN ACOMP Case\r\n");

    printf("acomp0 channel %u && %d\r\n",acompCfg0.posChanSel,acompCfg0.negChanSel);
    printf("acomp1 channel %u && %d\r\n",acompCfg1.posChanSel,acompCfg1.negChanSel);

    /* disable IRQ */
    bl_irq_disable(HBN_OUT1_IRQn);
    bl_irq_register_with_ctx(HBN_OUT1_IRQn, HBN_ACOMP01_IRQHandler_Cb, NULL);

    /* posedge/negedge */
    //HBN_Enable_AComp0_IRQ(HBN_ACOMP_INT_EDGE_POSEDGE);
    //HBN_Enable_AComp0_IRQ(HBN_ACOMP_INT_EDGE_NEGEDGE);
    HBN_Enable_AComp1_IRQ(HBN_ACOMP_INT_EDGE_POSEDGE);
    // HBN_Enable_AComp1_IRQ(HBN_ACOMP_INT_EDGE_NEGEDGE);
                
                        
    /* Notice: BL602 has no ACOMP GPIO fun sel, it use gpip channel */
    ACOMP_GPIO_Init();
    //AON_ACOMP_Init(AON_ACOMP0_ID,&acompCfg0)
    AON_ACOMP_Init(AON_ACOMP1_ID,&acompCfg1);
    //AON_ACOMP_Enable(AON_ACOMP0_ID);
    AON_ACOMP_Enable(AON_ACOMP1_ID);
    //HBN_Clear_IRQ(HBN_INT_ACOMP0);
    HBN_Clear_IRQ(HBN_INT_ACOMP1);

    //HBN_Enable_RTC_Counter(); 
    //HBN_Get_RTC_Timer_Val(&vallow, &valhig);
    //printf("low:%lu,hig:%lu\r\n", vallow, valhig);  
    /* enable IRQ */
    bl_irq_enable(HBN_OUT1_IRQn);
    for(uint32_t i = 0;i < 10;i++){
        printf("acomp0 normal status is %d, acomp1 normal status is %d\r\n",AON_ACOMP_Get_Result(AON_ACOMP0_ID),AON_ACOMP_Get_Result(AON_ACOMP1_ID));
        BL602_Delay_MS(100);
    }
    __disable_irq();
    printf("enter HBN\r\n");
    BL602_Delay_MS(10);

    cfg.gpioWakeupSrc = HBN_WAKEUP_GPIO_ALL;
    
    HBN_Clear_IRQ(HBN_INT_GPIO7);
    HBN_Clear_IRQ(HBN_INT_GPIO8);
    HBN_Clear_IRQ(HBN_INT_RTC);
    HBN_Clear_IRQ(HBN_INT_PIR);
    HBN_Clear_IRQ(HBN_INT_BOR);
    HBN_Clear_IRQ(HBN_INT_ACOMP0);
    HBN_Clear_IRQ(HBN_INT_ACOMP1);

    HBN_Mode_Enter_Ext(&cfg);
    BL602_Delay_MS(1000);
    printf("never reach here\r\n");
}

static void HBN_GPIO_IRQHandler_Cb(void)
{
    HBN_Clear_IRQ(HBN_INT_GPIO7);
    HBN_Clear_IRQ(HBN_INT_GPIO8);
}

void demo_hbn_gpio(void)
{
    
    SPI_Flash_Cfg_Type *flash_cfg;
    flash_cfg = bl_flash_get_flashCfg();
    
    HBN_APP_CFG_Type cfg={
        .useXtal32k=0,                                        /*!< Wheather use xtal 32K as 32K clock source,otherwise use rc32k */
        .sleepTime= 0,                                         /*!< HBN sleep time */
        .gpioWakeupSrc=HBN_WAKEUP_GPIO_8,                   /*!< GPIO Wakeup source */
        .gpioTrigType=HBN_GPIO_INT_TRIGGER_SYNC_FALLING_EDGE, /*!< GPIO Triger type */
        .flashCfg=flash_cfg,                                       /*!< Flash config pointer, used when power down flash */
        .hbnLevel=HBN_LEVEL_0,                                /*!< HBN level */
        .ldoLevel=HBN_LDO_LEVEL_1P10V,                        /*!< LDO level */
    };

    /* disable IRQ */
    bl_irq_disable(HBN_OUT1_IRQn);
    bl_irq_register_with_ctx(HBN_OUT1_IRQn, HBN_GPIO_IRQHandler_Cb, NULL);
                
                        
    /* Notice: BL602 has no ACOMP GPIO fun sel, it use gpip channel */
    ACOMP_GPIO_Init();
    /* enable IRQ */
    bl_irq_enable(HBN_OUT1_IRQn);

    __disable_irq();
    printf("enter HBN\r\n");
    BL602_Delay_MS(10);

    cfg.gpioWakeupSrc = HBN_WAKEUP_GPIO_ALL;
    
    HBN_Clear_IRQ(HBN_INT_GPIO7);
    HBN_Clear_IRQ(HBN_INT_GPIO8);
    HBN_Clear_IRQ(HBN_INT_RTC);
    HBN_Clear_IRQ(HBN_INT_PIR);
    HBN_Clear_IRQ(HBN_INT_BOR);
    HBN_Clear_IRQ(HBN_INT_ACOMP0);
    HBN_Clear_IRQ(HBN_INT_ACOMP1);

    HBN_Mode_Enter_Ext(&cfg);
    BL602_Delay_MS(1000);
    printf("never reach here\r\n");
}


void demo_hbn_set_magic(void) 
{
    *CONF_HBN_MAGIC_ADDR = CONF_ENABLE_HBN_MAGIC;
}

int demo_hbn_is_wakup_from_hbn(void) 
{
    uint32_t magic = *CONF_HBN_MAGIC_ADDR;

    *CONF_HBN_MAGIC_ADDR = 0;
    if ( magic == CONF_ENABLE_HBN_MAGIC) {
        return 1;
    }

    return 0;
}

static void cmd_hbn_acomp(char *buf, int len, int argc, char **argv)
{
    extern void demo_hbn_acomp();
    demo_hbn_acomp();
}

static void cmd_hbn_gpio(char *buf, int len, int argc, char **argv)
{
    extern void demo_hbn_gpio(void);

    *CONF_HBN_MAGIC_ADDR = CONF_ENABLE_HBN_MAGIC;

    demo_hbn_gpio();
}

const static struct cli_command cmds_user[] STATIC_CLI_CMD_ATTRIBUTE =
{
    {"hbn_acomp", "acomp hbn entry", cmd_hbn_acomp},
    {"hbn_gpio", "gpio hbn entry", cmd_hbn_gpio},
};

int bl602_hbn_cli_init(void)
{
    return 0;
}
