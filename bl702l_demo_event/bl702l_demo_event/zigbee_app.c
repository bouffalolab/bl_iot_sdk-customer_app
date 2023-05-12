/*
 * Copyright (c) 2016-2023 Bouffalolab.
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
#if defined(CFG_ZIGBEE_ENABLE)
#include <FreeRTOS.h>
#include <semphr.h>
#include <timers.h>
#include <aos/yloop.h>
#include <hal_hwtimer.h>
#include <hal_gpio.h>
#include <hal_sys.h>
#include "bl702l_glb.h"
#include "bl_sec.h"
#include "bl_timer.h"
#include "bl_rtc.h"
#include "hal_boot2.h"
#include "hosal_ota.h"
#include "utils_string.h"
#if defined(CFG_ZIGBEE_CLI)
#include <cli.h>
#include "zb_stack_cli.h"
#endif
#include "zigbee_app.h"
#include "zb_mac.h"
#include "zb_nwk.h"
#include "zb_zdp.h"
#include "zb_aps.h"
#include "zb_common.h"
#include "zcl_common.h"
#include "zcl_basic.h"
#include "zcl_powerCfg.h"
#include "zcl_onOff.h"
#include "zcl_level.h"
#include "zcl_identify.h"
#include "zcl_color.h"
#include "zcl_ota.h"
#include "zcl_groups.h"
#include "zcl_scenes.h"
#include "zcl_iasZone.h"
#include "zcl_touchlink.h"
#include "zcl_pollCtrl.h"
#include "zcl_diagnostics.h"
#include "zcl_windowCovering.h"
#include "zcl_tempMeasure.h"
#include "zcl_relHumidityMeasure.h"
#include "zcl_multistateInputBasic.h"
#include "zcl_electricalMeasurement.h"
#include "zcl_occupancySensing.h"
#include "zcl_metering.h"
#include "zcl_custom.h"
#include "zcl_illuminanceMeasurement.h"

#if defined(CFG_ZIGBEE_PDS)
#include "rom_zb_simple_ext.h"
#endif
#if defined(CFG_ZIGBEE_HBN)
#include "bl_hbn.h"
#endif

#define ZB_LONG_POLL_INTVL_QUARTER_SECS    4 //4 QuarterSeconds = 1 second
#define ZB_SHORT_POLL_INTVL_QUARTER_SECS   2 //2 QuarterSeconds = 500 ms

zb_appCb appCb;
uint8_t otaClientEp = 0xff;
TimerHandle_t otaTimerHdl;
TimerHandle_t zbStartTimerHdl;
#define ZCL_OTA_GET_PROGRESS_INTVL_IN_SEC 10
#define ZB_START_INTVL_IN_SEC 4
uint8_t gEp;

// Extend this macro by Zone Table, maintenance by user if needed
// Here, use fixed zone id 1
#define IAS_ZONE_FIXED_ID  1
bool otaOngoing = false;
#if defined(CFG_ZIGBEE_PDS) && ZB_SIMPLE_MODE_ENABLE && !defined(CFG_ZIGBEE_HBN)
extern struct _blZbPdsEnv blZbPdsEnv;;
#endif

void zb_startTimerHandler(TimerHandle_t p_timerhdl)
{
    uint8_t randomDelay = 0;
    uint8_t timeoutInSec = 0;
    
    if(p_timerhdl)
    {
        xTimerDelete(p_timerhdl, 0);
        zbStartTimerHdl = NULL;
    }
    if(!zb_isDevActiveInNwk())
    {
        if(!zcl_touchlinkTarInProgress())
        {
            printf("Do zb_start\r\n");
            zb_start();
        }
        else
        {
            printf("touchlink is ongoing\r\n");
            randomDelay = bl_rand()%3;
            timeoutInSec = ZB_START_INTVL_IN_SEC + randomDelay;
            zbStartTimerHdl = xTimerCreate("Timer", pdMS_TO_TICKS(timeoutInSec * 1000), 0, NULL,  zb_startTimerHandler);
            if(zbStartTimerHdl)
                xTimerStart(zbStartTimerHdl, 0);    
        }
    }
}

#if defined(CFG_ZIGBEE_HBN)
#define ZB_HBN_START_TIMEOUT_SEC 5
extern bool hbn_start;
TimerHandle_t zbHbnStartTimerHdl;
extern struct _zbRxPacket rxPktStoredInHbnRam;
extern struct _zsedNwkInfo zsedNwkInfo;
extern struct _zclReportInfo zclReportInfoList[ZB_APP_ZCL_REPORT_CNT_MAX];
static void zb_send_attr_report(uint8_t repIdx)
{
     struct _zclTxParam zclTxParam;
     struct _zclReportAttrReportRec attrRec;
     uint8_t dataSize = 0;
     zbRet_t ret = ZB_SUCC;
 	 memset(&zclTxParam, 0, sizeof(struct _zclTxParam));
     memset(&attrRec, 0, sizeof(struct _zclReportAttrReportRec));
     zclTxParam.radius = 0;
     zclTxParam.dstAddrMode = ZB_APS_ADDR_MODE_INDIRECT;
     zclTxParam.srcEp = gEp;
     zclTxParam.disZclDefaultResp = true;
     zclTxParam.dir = 1;
     zclTxParam.manufactureSpecific = false;
     zclTxParam.txOptions |= ZB_APS_TX_OPTIONS_ACK_TRANS;
     zclTxParam.profileId = ZB_PROFILE_ID_HA;
     attrRec.attrId = zclReportInfoList[repIdx].record.attrId;
     attrRec.attrDataType =  zclReportInfoList[repIdx].record.dataType;
     dataSize = zcl_getAttrSizeByType(zclReportInfoList[repIdx].record.dataType, NULL);
     uint8_t data[dataSize];
     if(zcl_getAttr(zclReportInfoList[repIdx].clustId, attrRec.attrId, data, &dataSize, gEp) == ZB_SUCC)
     {
         attrRec.attrData = data;
         ret = zcl_sendReportAttr(&zclTxParam, zclReportInfoList[repIdx].clustId, 1, &attrRec, NULL);
         if(ret)
            printf("%s fail to send report attribute with error (0x%x)\r\n", __func__,ret);
     }
     zclReportInfoList[repIdx].nextReportBaseCnt = bl_rtc_get_counter();
}

bool zcl_checkIfTriggerAttrReport(bool bReport)
{
    extern uint8_t totalReportCnt;
    for(int i=0; i < totalReportCnt; i++)
    { 
        if(!zclReportInfoList[i].nextReportDeltaMs)
            continue;
        
        if(bl_rtc_get_delta_time_ms(zclReportInfoList[i].nextReportBaseCnt) >= zclReportInfoList[i].nextReportDeltaMs)
        {
            //nextReportIndx = i;
            if(!bReport)
                return true;
            zb_send_attr_report(i);
        }
    }

    return false;
}

static bool zb_macIndCb(struct _zbMacIndication *macInd)
{
    uint16_t frameCtrl;
    uint8_t panIdCompress;
    uint8_t dstAddrMode;
    uint8_t srcAddrMode;
    uint16_t dstShortAddr;
    uint16_t srcShortAddr;
    uint16_t dstPanId;
    bool bResetTimer = false;

    //if ota is ongoing, device cannot enter into hbn. No need to reset the hbnStartTimer
    if(otaOngoing)
        return true;
    
    if(!macInd || macInd->psduLength < 3)
        return true;

    frameCtrl = macInd->psdu[0] | (macInd->psdu[1] << 8);
    panIdCompress = (frameCtrl & ZB_MAC_PAN_ID_COMPRESS_MASK)>>ZB_MAC_PAN_ID_COMPRESS_OFFSET;
    dstAddrMode = (frameCtrl & ZB_MAC_DST_ADDR_MODE_MASK)>>ZB_MAC_DST_ADDR_MODE_OFFSET;
    srcAddrMode = (frameCtrl & ZB_MAC_SRC_ADDR_MODE_MASK)>>ZB_MAC_SRC_ADDR_MODE_OFFSET;

    if(panIdCompress && dstAddrMode == ZB_MAC_16_BIT_ADDR_MODE  && srcAddrMode == ZB_MAC_16_BIT_ADDR_MODE)
    {
        dstPanId = (macInd->psdu[4] << 8)| macInd->psdu[3];
        if(zb_getPanId() == dstPanId)
        {
            dstShortAddr = (macInd->psdu[6] << 8)| macInd->psdu[5];
            srcShortAddr = (macInd->psdu[8] << 8)| macInd->psdu[7];
            if(zb_getShortAddr()== dstShortAddr && zb_getParentShortAddr() == srcShortAddr)
                bResetTimer = true;    
            
        }
    }  

    taskENTER_CRITICAL();
    if(bResetTimer && zbHbnStartTimerHdl && xTimerIsTimerActive(zbHbnStartTimerHdl))
    {
        //printf("%s, reset timer\r\n", __func__);
        xTimerStop(zbHbnStartTimerHdl, 0);
        xTimerStart(zbHbnStartTimerHdl, 0);
    }
    taskEXIT_CRITICAL();
    return true;
}

static void zb_hbnStartTimerHandler(TimerHandle_t p_timerhdl)
{
    if(p_timerhdl)
    {
        xTimerDelete(p_timerhdl, 0);
        zbHbnStartTimerHdl = NULL;
    }

    if(otaOngoing == false && zb_isDevActiveInNwk() && zb_isAuthenticated()){
        hbn_start = true;
    }else{
       // stop hbn during ota process 
    }

}

static void zb_createHbnStartTimer(void)
{
    zbHbnStartTimerHdl = xTimerCreate("Timer", pdMS_TO_TICKS(ZB_HBN_START_TIMEOUT_SEC * 1000), 0, NULL,  zb_hbnStartTimerHandler);
    if(zbHbnStartTimerHdl)
        xTimerStart(zbHbnStartTimerHdl, 0);
}
#endif

#define ZB_SAMPLE_TEST_ENABLE 0
#if ZB_SAMPLE_TEST_ENABLE
#define ZB_SAMPLE_START_TIMEOUT_SEC 5
TimerHandle_t zbSampleStartTimerHdl;
static void zb_sampleStartTimerHandler(TimerHandle_t p_timerhdl)
{
    printf("%s\r\n", __func__);
    xTimerStart(p_timerhdl, 0);
}

static void zb_createSampleStartTimer(void)
{
    zbSampleStartTimerHdl = xTimerCreate("Timer", pdMS_TO_TICKS(ZB_SAMPLE_START_TIMEOUT_SEC * 1000), 0, NULL,  zb_sampleStartTimerHandler);
    if(zbSampleStartTimerHdl)
        xTimerStart(zbSampleStartTimerHdl, 0);
}
#endif
void zb_nwkStartupCompleteCb(const uint8_t status, const uint8_t roleType)
{
    uint8_t timeoutInSec = 0;
    uint8_t randomDelay = 0;
    printf("zb_startup_complete_cb: 0x%x, 0x%x\r\n", status, roleType);
    if(status == 0x00)
    {
        if(zbStartTimerHdl)
        {
             xTimerDelete(zbStartTimerHdl, 0);
             zbStartTimerHdl = NULL;    
        }
        
        if (roleType == ZB_ROLE_SLEEPY_ENDDEVICE)
        {
            printf("Start as ZSED: 0x%2x\r\n", zb_getShortAddr());
            #if ZB_SAMPLE_TEST_ENABLE
            zb_createSampleStartTimer();
            #endif
            #if defined(CFG_ZIGBEE_HBN)
            hbn_start = false;
            zb_registerMacIndCb(zb_macIndCb);
            if(!zbHbnStartTimerHdl)
                zb_createHbnStartTimer();
            if(rxPktStoredInHbnRam.len)
            {
                printf("notify rx packet stored in hbn ram, zsedNwkInfo.macSeqNum=%d\r\n", zsedNwkInfo.macSeqNum);
                uint16_t fcs = (rxPktStoredInHbnRam.fcs[1] << 8)|rxPktStoredInHbnRam.fcs[0];
                zb_notifyRxPacket(rxPktStoredInHbnRam.data, rxPktStoredInHbnRam.len,  fcs, rxPktStoredInHbnRam.freqOffset, 
                                        rxPktStoredInHbnRam.rssi, rxPktStoredInHbnRam.sfdCorr);
                memset(&rxPktStoredInHbnRam, 0, sizeof(struct _zbRxPacket));
            }

            zcl_checkIfTriggerAttrReport(true);
            #endif
        }
        else if (roleType == ZB_ROLE_ROUTER)
        {
            printf("Start as ZR: 0x%2x\r\n", zb_getShortAddr());
        }
        else if (roleType == ZB_ROLE_COORDINATOR)
        {
            printf("Start as ZC: 0x%2x\r\n", zb_getShortAddr());
        }
        else if (roleType == ZB_ROLE_NONSLEEPY_ENDDEVICE)
        {
            printf("Start as ZNSED: 0x%2x\r\n", zb_getShortAddr());
        }
    }
    else
    {
        if(appCb)
            printf("zb startup fail: 0x%x\r\n", status);
        else
        {
            printf("zb startup fail with status 0x%x\r\n", status);
            randomDelay = bl_rand()%3;
            timeoutInSec = ZB_START_INTVL_IN_SEC + randomDelay;
            //application layer can control the interval between attempts, e.g.start a timer 
            zbStartTimerHdl = xTimerCreate("Timer", pdMS_TO_TICKS(timeoutInSec * 1000), 0, NULL,  zb_startTimerHandler);
            if(zbStartTimerHdl)
                xTimerStart(zbStartTimerHdl, 0);
        }
    }

    if(appCb)
        (appCb)(status, roleType);
}

//clean all timers and flags
void zb_leaveProcess(){

    // stop ota timer and flag
    if(otaTimerHdl) {
        xTimerDelete(otaTimerHdl, 0);
        otaTimerHdl = 0;
    }

    if(otaOngoing){
        otaOngoing = false;
    }
    #if defined(CFG_ZIGBEE_HBN)
    taskENTER_CRITICAL();
    // stop hbn timer
    if(zbHbnStartTimerHdl)
    {
        xTimerDelete(zbHbnStartTimerHdl, 0);
        zbHbnStartTimerHdl = NULL;
    }
    taskEXIT_CRITICAL();
    #endif
}

void zb_nwkLeaveCb(const uint8_t status, const bool rejoin)
{
    printf("zb_nwk_leave_cb (status:0x%x) (rejoin:%d)\r\n\r\n", status, rejoin);
    
    zb_leaveProcess();
}

void zc_getNewJoinedDeviceAddr(const uint16_t shortAddr)
{
    printf("The address joined is 0x%2x\r\n", shortAddr);
}

bool zb_apsIndCb(struct _zbApsdeDataIndication *apsInd)
{
    //printf("aps indication: srcAddr=%04x, srcEp=%d, dstAddr=%04x, dstEp=%d, profileId=%04x, clusterId=%04x\r\n", 
    //  apsInd->srcAddr.shortAddr, apsInd->srcEp, apsInd->dstAddr.shortAddr, apsInd->dstEp, apsInd->profileId, apsInd->clusterId);
        
    return true;
}

void zb_apsConfCb(struct _zbApsdeDataConfirm *apsConf)
{
    /*
    printf("aps confirm: status=0x%02x, dstAddr=%04x, dstEp=%d, srcEp=%d, tag=%lu\r\n", 
        apsConf->status, apsConf->dstAddr.shortAddr, apsConf->dstEp, apsConf->srcEp, apsConf->tag);
    */
}


/*
* Zigbee event callback handler
*/
zbRet_t zb_eventHandler(uint8_t evtId, uint8_t * evtParam)
{
    switch(evtId)
    {
        case ZB_EVT_STARTUP_COMPLETE:
        {
            zb_nwkStartupCompleteCb(evtParam[0], evtParam[1]);
            break;
        }
        case ZB_EVT_NWK_LEAVE:
        {
            zb_nwkLeaveCb(evtParam[0], evtParam[1]);
            break;
        }
        case ZB_EVT_AUTH:
        {
            uint16_t shortAddr = (uint16_t)evtParam[0] + (((uint16_t)evtParam[1]<<8)&0xff00);
            zc_getNewJoinedDeviceAddr(shortAddr);
            break;
        }
        default:
            break;
    }

    return ZB_SUCC;
}

/*
* ZCL Callback
*/

zbRet_t zcl_onoffEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam)
{
    switch(evtId)
    {
        case ZCL_EVT_ONOFF_UPDATE_ONOFF:
        {
            // add PWM/LED driver code here
            if(evtParam[0] == 1)
            {
                //hal_gpio_led_on();
                printf("OnOff is On\r\n");
            }
            else
            {
                //hal_gpio_led_off();
                printf("OnOff is Off\r\n");
            }
            break;
        }
    }

    return ZB_SUCC;
}

zbRet_t zcl_levelControlEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam)
{
    switch(evtId)
    {
        case ZCL_EVT_LEVEL_UPDATE_CURR_LEVEL:
        {
            // add PWM/LED driver code here
            printf("Current Level is 0x%x\r\n", evtParam[0]);
            break;
        }
        case ZCL_EVT_CLIENT_CMD_RECEIVED:
        {
            printf("Level Control Command Received is: 0x%x\r\n", evtParam[0]);
            printf("Payload length: %d\r\n", evtParam[1]);
            for(int i=0; i<evtParam[1]; i++)
            {
                printf("Payload[%d]:0x%x\r\n", i, evtParam[i+2]);
            }
            break;
        }
    }

    return ZB_SUCC;    
}

zbRet_t zcl_scenesEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam)
{
    switch(evtId)
    {
        case ZCL_EVT_CLIENT_CMD_RECEIVED:
        {
            printf("Scenes Command Received is: 0x%x\r\n", evtParam[0]);
            printf("Payload length: %d\r\n", evtParam[1]);
            for(int i=0; i<evtParam[1]; i++)
            {
                printf("Payload[%d]:0x%x\r\n", i, evtParam[i+2]);
            }
            break;
        }
    }

    return ZB_SUCC;    
}

/*
* evtParam[0-1] : identify time value; 
*/

zbRet_t zcl_IdentifyEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam)
{
    switch(evtId)
    {
        case ZCL_EVT_IDENTIFY_UPDATE_IDENTIFY_TIME:
        {
            // add customer application code here
            uint16_t identifyTime = (uint16_t)evtParam[0] | (uint16_t)(evtParam[1]<<8);
            printf("identify time is %d\r\n", identifyTime);
            break;
        }
        case ZCL_EVT_CLIENT_CMD_RECEIVED:
        {
            printf("Identify Cluster Command Received is: 0x%x\r\n", evtParam[0]);
            printf("Payload length: %d\r\n", evtParam[1]);
            for(int i=0; i<evtParam[1]; i++)
            {
                printf("Payload[%d]:0x%x\r\n", i, evtParam[i+2]);
            }
            break;
        }
    }

    return ZB_SUCC;    
}

zbRet_t zcl_colorControlEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam)
{
    printf("zcl_colorControlEventHandler  evtId: 0x%02x\r\n", evtId);
    switch(evtId)
    {
        case ZCL_EVT_COLOR_UPDATE_CURRENT_HUE:
        {
            uint8_t currentHue = (uint8_t)evtParam[0];
            printf("currentHue is: 0x%02x\r\n", currentHue);
        break;
        }

        case ZCL_EVT_COLOR_UPDATE_CURRENT_SATURATION:
        {
            uint8_t currentSaturation = (uint8_t)evtParam[0];
            printf("currentSaturation is: 0x%02x\r\n", currentSaturation);
        break;
        }

        case ZCL_EVT_COLOR_UPDATE_CURRENT_X:
        {
            uint16_t currentX = (uint16_t)evtParam[0] | (uint16_t)(evtParam[1]<<8);
            printf("currentX is: 0x%04x\r\n", currentX);
        break;
        }

        case ZCL_EVT_COLOR_UPDATE_CURRENT_Y:
        {
            uint16_t currentY = (uint16_t)evtParam[0] | (uint16_t)(evtParam[1]<<8);
            printf("currentY is: 0x%04x\r\n", currentY);
        break;
        }

        case ZCL_EVT_COLOR_UPDATE_TEMP_MIREDS:
        {
            uint16_t colorTempMireds = (uint16_t)evtParam[0] | (uint16_t)(evtParam[1]<<8);
            printf("colorTempMireds is: 0x%04x\r\n", colorTempMireds);
        break;
        }

        case ZCL_EVT_COLOR_UPDATE_ENHANCEDCURRENT_HUE:
        {
            uint16_t enhancedCurrentHue = (uint16_t)evtParam[0] | (uint16_t)(evtParam[1]<<8);
            printf("enhancedCurrentHue is: 0x%04x\r\n", enhancedCurrentHue);
        break;
        }
        case ZCL_EVT_CLIENT_CMD_RECEIVED:
        {
            printf("Color Control Command Received is: 0x%x\r\n", evtParam[0]);
            printf("Payload length: %d\r\n", evtParam[1]);
            for(int i=0; i<evtParam[1]; i++)
            {
                printf("Payload[%d]:0x%x\r\n", i, evtParam[i+2]);
            }
            break;
        }

        default: break;
    }

    return ZB_SUCC;    
}

zbRet_t zcl_clustReportHandler(uint16_t clustId, uint8_t srcEp, uint16_t srcAddr, 
                       uint8_t *zclPayload, uint8_t zclPayloadLen)
{
    printf("Report 0x%2x, ep is %d, Addr is 0x%2x\r\n", clustId, srcEp, srcAddr);
    for(uint8_t i = 0; i < zclPayloadLen; i++)
    {
        printf("zclPayload[%d]:0x%x\r\n",i, zclPayload[i]);
    }

    return ZB_SUCC; 
}

//This is just for example, in which, it read ota image data from the "mfg" partition.
//Ota image can be downloaded to flash as "MFG Bin" using BouffaloLab Dev Cube. 
zbRet_t zcl_otaLoadLocalOTAImage()
{
#if 0
    uint32_t imageOffset = 0x1000;
    uint32_t otaPartitionSize = 0;
    uint32_t otaPartitionStartAddress = 0;
    if(hal_boot2_partition_addr_inactive("FW",&otaPartitionStartAddress,&otaPartitionSize))
    {
        printf("get ota partition failed\r\n");
        return ZB_ERR_FAIL;
    }
                           
    if(hosal_ota_start(otaPartitionSize))
    {
        printf("hosal_ota_start() failed\r\n");
        return ZB_ERR_FAIL;
    }
                       
    struct _zclOtaHeader header;
    if(hosal_ota_read(imageOffset, (uint8_t *)&header, sizeof(struct _zclOtaHeader)))
    {
        printf("read ota header from flash failed\r\n");
        return ZB_ERR_FAIL;
    }
                       
    return zcl_otaLoadNewImage(&header);
    #endif
    return 0;
}


struct _zclOTAImageBlockRespSettings imgBlockRespSettings;

zbRet_t zcl_otaSetImageBlockResp(uint8_t status, uint32_t currentTime, uint32_t requestTime, uint16_t minBlockPeriod)
{
    imgBlockRespSettings.status = status;
    imgBlockRespSettings.currentTime = currentTime;
    imgBlockRespSettings.requestTime = requestTime;
    imgBlockRespSettings.minBlockPeriod = minBlockPeriod;

    return ZB_SUCC;
}

#if defined(CFG_ZIGBEE_CLI)
void zbcli_zcl_ota_load_ota_image(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
    int ret = 0;
    
    ret = zcl_otaLoadLocalOTAImage();
    if(ret)
        printf("Failed to load ota image, error_code(0x%x)\r\n", ret);
}

void zbcli_zcl_ota_set_image_block_resp(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
    uint8_t para_cnt = PRE_CLI_PARAM_NUM;
    uint8_t status = 0;
    uint32_t currentTime = 0;
    uint32_t requestTime = 0;
    uint16_t minBlockPeriod = 0;

    if(argc != (para_cnt  + 1) && argc != (para_cnt + 4))
    {
        printf("Wrong number of args\r\n");
        printf("zb_zcl_ota_set_image_block_resp <status> <current_time> <request_time> <minimum_block_period>\r\n"
               "status - <uint8_t> image block resp status(0x00:success, 0x95:abort, 0x97:wait for data)\r\n"
               "current_time - <uint32_t> (for status=0x97) server's current time\r\n"
               "request_time - <uint32_t> (for status=0x97) the request time that the client SHALL retry the request command\r\n"
               "minimum_block_period - <uint16_t> (for status=0x97) the minimum delay between image block requests, in milliseconds\r\n"
               );
        return;
    }
    else
    {
        get_uint8_from_string(&argv[para_cnt++], &status);

        if(status != 0x00 && status != 0x95 && status != 0x97)
        {
            printf("unsupported input status value\r\n");
            return;
        }

        if(status == 0x97)
        {
            get_uint32_from_string(&argv[para_cnt++], &currentTime);
            get_uint32_from_string(&argv[para_cnt++], &requestTime);
            get_uint16_from_string(&argv[para_cnt++], &minBlockPeriod);
        }

        zcl_otaSetImageBlockResp(status, currentTime, requestTime, minBlockPeriod);     
    }
}
#endif
zbRet_t zcl_otaCommandSentCb(struct _zclRespCbHdr *respCbHdr, union _zclResp *respMessage)
{
    if(respCbHdr->flag.BF.isApsConfirm)
    {
        printf("command sent confirm status: 0x%02x\r\n", respCbHdr->status);
    }

    return ZB_SUCC;
}

void zcl_otaGenImageBlockResp(struct _zclRecvdReqAddressInfo *reqAddress, struct _zclOTAImageBlockReq *imgBlockReq)
{
#if 0
    int ret = 0;
    uint32_t imageOffset = 0x1000;
    uint8_t buf[128];
    union _zclOTAImageBlockResp resp;
    struct _zclTxParam apsParam;
    struct _zclRecvdReqAddressInfo addressInfo;

    memcpy((uint8_t*)&addressInfo, (uint8_t*)reqAddress, sizeof(struct _zclRecvdReqAddressInfo));

    memset(&apsParam, 0x00, sizeof(struct _zclTxParam));
    apsParam.dstAddrMode = ZB_APS_ADDR_MODE_SHORT;
    apsParam.dstAddr.shortAddr = addressInfo.srcShortAddr;
    apsParam.srcEp = addressInfo.dstEndpoint;
    apsParam.dstEp = addressInfo.srcEndpoint;
    apsParam.profileId = ZB_PROFILE_ID_HA;
    apsParam.txOptions = ZB_APS_TX_OPTIONS_ACK_TRANS;
    apsParam.dir = 1;
    apsParam.disZclDefaultResp = true;

    switch(imgBlockRespSettings.status)
    {
        case ZCL_STATUS_ABORT:
            resp.abortResp.status = ZCL_STATUS_ABORT;
            break;
        case ZCL_STATUS_WAIT_FOR_DATA:
            resp.waitForData.status = ZCL_STATUS_WAIT_FOR_DATA;
            resp.waitForData.currentTime = imgBlockRespSettings.currentTime;
            resp.waitForData.requestTime = imgBlockRespSettings.requestTime;
            resp.waitForData.minBlockPeriod = imgBlockRespSettings.minBlockPeriod;
            break;
        case ZCL_STATUS_SUCCESS:
        default:
            hosal_ota_read(imgBlockReq->fileOffset + imageOffset, buf, imgBlockReq->maxDataSize);
            resp.successResp.status = ZCL_STATUS_SUCCESS;
            resp.successResp.manufactureCode = imgBlockReq->manufactureCode;
            resp.successResp.imageType = imgBlockReq->imageType;
            resp.successResp.fileVersion = imgBlockReq->fileVersion;
            resp.successResp.fileOffset = imgBlockReq->fileOffset;
            resp.successResp.dataSize = imgBlockReq->maxDataSize;
            break;
    }

    ret = zcl_otaSendImageBlockResp(&apsParam, &resp, (imgBlockRespSettings.status !=  ZCL_STATUS_SUCCESS) ? NULL : buf, zcl_otaCommandSentCb);
    if(ret){
        printf("failed to image block response, ret=%d\r\n", ret);
    }
 #endif
}

void zcl_otaGenUpgradeEndResp(struct _zclRecvdReqAddressInfo *reqAddress, struct _zclOTAUpgradeEndReq *upgradeEndReq)
{
    struct _zclOTAUpgradeEndResp upgradeEndResp;
    struct _zclTxParam apsParam;
    struct _zclRecvdReqAddressInfo addressInfo;

    memcpy((uint8_t*)&addressInfo, (uint8_t*)reqAddress, sizeof(struct _zclRecvdReqAddressInfo));

    memset(&apsParam, 0x00, sizeof(struct _zclTxParam));
    apsParam.dstAddrMode = ZB_APS_ADDR_MODE_SHORT;
    apsParam.dstAddr.shortAddr = addressInfo.srcShortAddr;
    apsParam.srcEp = addressInfo.dstEndpoint;
    apsParam.dstEp = addressInfo.srcEndpoint;
    apsParam.profileId = ZB_PROFILE_ID_HA;
    apsParam.txOptions = ZB_APS_TX_OPTIONS_ACK_TRANS;
    apsParam.dir = 1;
    apsParam.disZclDefaultResp = true;

    upgradeEndResp.manufactureCode = upgradeEndReq->manufactureCode;
    upgradeEndResp.imageType = upgradeEndReq->imageType;
    upgradeEndResp.fileVersion = upgradeEndReq->fileVersion;
    upgradeEndResp.currentTime = 0;
    upgradeEndResp.upgradeTime = 5;

    int ret = zcl_otaSendUpgradeEndResp(&apsParam, &upgradeEndResp, zcl_otaCommandSentCb);

    if(ret){
        printf("failed to upgrade end response, ret=%d\r\n", ret);
    }
}

void  zcl_checkOtaProgress(TimerHandle_t p_timerhdl)
{
    uint32_t percentage = 0;
    uint32_t currOffset = 0;
    uint32_t imageSize = 0;
    zcl_otaGetProgress(otaClientEp, &currOffset, &imageSize, &percentage);
    printf("OTA Progress[%d][%ld%%]: %ld / %ld\r\n", otaClientEp, percentage, currOffset, imageSize);
    xTimerStart(p_timerhdl, 0);
}

zbRet_t zcl_otaEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam)
{
    printf("%s, evtId %d, ep %d\r\n", __func__,evtId, ep);
    switch(evtId)
    {
        case ZCL_EVT_OTA_START:
        {
            printf("%s, ota start", __func__);
            otaClientEp = ep;
            otaOngoing = true;
            #if defined(CFG_ZIGBEE_HBN)
            hbn_start = false;
            taskENTER_CRITICAL();
            if(zbHbnStartTimerHdl && xTimerIsTimerActive(zbHbnStartTimerHdl))
            {
                xTimerStop(zbHbnStartTimerHdl, 0);
            }
            taskEXIT_CRITICAL();
            #endif

            otaTimerHdl = xTimerCreate("Timer", pdMS_TO_TICKS(ZCL_OTA_GET_PROGRESS_INTVL_IN_SEC * 1000), 0, NULL,  zcl_checkOtaProgress);
            if(otaTimerHdl)
                xTimerStart(otaTimerHdl, 0);
        }
        break;
   
        case ZCL_EVT_OTA_ABORT:
        {
            printf("ota abort, reason=%d\r\n", evtParam[0]);
            if(otaTimerHdl) {
                xTimerDelete(otaTimerHdl, 0);
                otaTimerHdl = 0;
            }
          
            otaOngoing = false;
            #if defined(CFG_ZIGBEE_HBN)
            taskENTER_CRITICAL();
            if(zbHbnStartTimerHdl && !xTimerIsTimerActive(zbHbnStartTimerHdl))
            {
                xTimerStart(zbHbnStartTimerHdl, 0);
            }
            taskEXIT_CRITICAL();
            #endif
        }
        break;

        case ZCL_EVT_OTA_CHECK_SIGNATURE_SIGNER:
        {
            printf("signature signer IEEE address:");
            for(uint8_t i=0;i<8;i++)
            {
                printf("%02x", evtParam[i]);
            }
            printf("\r\n");
        }
        break;
        case ZCL_EVT_OTA_CHECK_CERTIFICATE_ISSUER:
        {
            printf("certificate issuer IEEE address:");
            for(uint8_t i=0;i<8;i++)
            {
                printf("%02x", evtParam[i]);
            }
            printf("\r\n");
        }
        break;
   
        case ZCL_EVT_OTA_IMAGE_CMPLT:
        { 
            uint32_t percentage = 0;
            uint32_t currOffset = 0;
            uint32_t imageSize = 0;
            zcl_otaGetProgress(otaClientEp, &currOffset, &imageSize, &percentage);
            printf("OTA Progress[%d][%ld%%]: %ld / %ld\r\n", otaClientEp, percentage, currOffset, imageSize);
            if(evtParam[0] == ZCL_OTA_STATUS_SUCC)
                printf("New image reception is completed successfully\r\n");
            else if(evtParam[0] == ZCL_OTA_STATUS_IMG_VERIFY_FAILED)
                printf("New image is not valid because of verification failure, errorCode=%d\r\n", evtParam[1]);

            if(otaTimerHdl) {
                xTimerDelete(otaTimerHdl, 0);
                otaTimerHdl = 0;
            }

            //stack will report ZCL_EVT_OTA_NEW_IMAGE_APPLY event to reboot device 
        }
        break;
   
        case ZCL_EVT_OTA_NEW_IMAGE_APPLY:
        {
            printf("To apply the new image\r\n");
            hal_reboot();
        }
        break;

        case ZCL_EVT_OTA_IMAGE_QUERY_REQUEST:
        {
            struct _zclRecvdReqAddressInfo reqAddress;
            struct _zclOTAQueryNextImageReq imgQueryReq;
            memcpy(&imgQueryReq, evtParam, sizeof(struct _zclOTAQueryNextImageReq));
            memcpy(&reqAddress, evtParam + sizeof(struct _zclOTAQueryNextImageReq), sizeof(struct _zclRecvdReqAddressInfo));
            printf("ImageQueryReq: srcAddr=0x%04x, fileVersion=0x%08lx, imageType=0x%04x, manufactureCode=0x%04x\r\n", 
                reqAddress.srcShortAddr, imgQueryReq.currentFileVersion, imgQueryReq.imageType, imgQueryReq.manufactureCode);
        }
        break;

        case ZCL_EVT_OTA_IMAGE_QUERY_RESPONSE:
        {
            struct _zclRecvdReqAddressInfo reqAddress;
            struct _zclOTAQueryNextImageResp imgQueryResp;
            memcpy(&imgQueryResp, evtParam, sizeof(struct _zclOTAQueryNextImageResp));
            memcpy(&reqAddress, evtParam + sizeof(struct _zclOTAQueryNextImageResp), sizeof(struct _zclRecvdReqAddressInfo));
            printf("ImageQueryResp: srcAddr=0x%04x, status=0x%02x\r\n", reqAddress.srcShortAddr, imgQueryResp.status);
        }
        break;

        case ZCL_EVT_OTA_IMAGE_BLOCK_REQUEST:
        {
            struct _zclRecvdReqAddressInfo reqAddress;
            struct _zclOTAImageBlockReq imgBlockReq;
            memcpy(&imgBlockReq, evtParam, sizeof(struct _zclOTAImageBlockReq));
            memcpy(&reqAddress, evtParam + sizeof(struct _zclOTAImageBlockReq), sizeof(struct _zclRecvdReqAddressInfo));
            printf("ImageBlockReq: srcAddr=0x%04x, offset=%lu\r\n", reqAddress.srcShortAddr, imgBlockReq.fileOffset);
            zcl_otaGenImageBlockResp(&reqAddress, &imgBlockReq);
        }
        break;

        case ZCL_EVT_OTA_UPGRADE_END_REQUEST:
        {
            struct _zclRecvdReqAddressInfo reqAddress;
            struct _zclOTAUpgradeEndReq upgradeEndReq;
            memcpy(&upgradeEndReq, evtParam, sizeof(struct _zclOTAUpgradeEndReq));
            memcpy(&reqAddress, evtParam + sizeof(struct _zclOTAUpgradeEndReq), sizeof(struct _zclRecvdReqAddressInfo));
            printf("UpgradeEndReq: srcAddr=0x%04x, status=0x%02x\r\n", reqAddress.srcShortAddr, upgradeEndReq.status);
            zcl_otaGenUpgradeEndResp(&reqAddress, &upgradeEndReq);
        }
        break;
   
        default:
            break;
    }

    return ZB_SUCC; 
}

static void zb_zcl_resp_error(uint8_t nStatus)
{
     switch(nStatus){
        case ZB_MAC_NO_ACK:
            printf("No mac ack\r\n");
        break;
        case ZB_NWK_ROUTE_ERROR:
            printf("nwk route error\r\n");
        break;
        case ZB_APS_NO_ACK:
            printf("aps no ack\r\n");
        break;
        case ZCL_STATUS_TIMEOUT:
            printf("zcl timeout\r\n");
        break;
        default:
            printf("Error code is 0x%x\r\n", nStatus);
     }
}

zbRet_t zcl_IasZoneEnrollRspCb(struct _zclRespCbHdr *respCbHdr, union _zclResp *respMessage)
{
     if(respCbHdr->flag.BF.isApsConfirm)
    {
        printf("recived aps_confirm 0x%x\r\n", respCbHdr->status);
        if(respCbHdr->status)
        {
            zb_zcl_resp_error(respCbHdr->status);
        }
        return ZB_SUCC;
    }

    if(respCbHdr->status)
    {
        printf("zcl status is 0x%x\r\n", respCbHdr->status);
        return ZB_SUCC;
    }

    if(respCbHdr->flag.BF.zclfrmtype == ZCL_FRAME_TYPE_CMD_GLOBAL)
    {
        printf("In response message,responderAddr:0x%02x, cmdId : 0x%02x, status : 0x%02x\r\n", 
                respCbHdr->responderAddr,
                respMessage->defaultResp.cmdId, 
                respMessage->defaultResp.status);
    }
    return ZB_SUCC;
}

zbRet_t zcl_iasZoneEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam)
{
    switch(evtId)
    {
        case ZCL_EVT_IASZONE_STATUS_CHANGE_NOTIFICATION:
        {
            const uint8_t cmdId = evtParam[0];
            const uint8_t evtParamDataLen = evtParam[1];
            const uint8_t srcEp = evtParam[2]; // command received src ep
            const uint16_t sourceAddr = (uint16_t)evtParam[3] + 
                                        (uint16_t)(evtParam[4] << 8 & 0xff00);

            const uint8_t expecetdEvtParamLen = 6;
            if(expecetdEvtParamLen != evtParamDataLen)
            {
                printf("IASZONE_STATUS_CHANGE_NOTIFICATION Payload Error\r\n");
                return ZB_ERR_INVALID_PARAM;
            }

            const uint16_t zoneStatus = evtParam[5] + ((uint16_t)evtParam[6]<<8);
            const uint8_t  extStatus = evtParam[7];
            const uint8_t  zoneId = evtParam[8];
            const uint16_t delay = evtParam[9] + ((uint16_t)evtParam[10]<<8);
            printf("IAS ZONE Status change notification [0x%04x : 0x%x]\r\n"
                   "CmdId : 0x%x, param len is 0x%x\r\n"
                   "zone status is 0x%04x\r\n"
                   "extStatus is 0x%x\r\n"
                   "zoneId is 0x%x\r\n"
                   "delay is 0x%04x\r\n",
                   sourceAddr, srcEp, cmdId, evtParamDataLen, 
                   zoneStatus, extStatus, zoneId, delay);

            return ZB_SUCC;
        }
        case ZCL_EVT_IASZONE_ENROLL_REQ_CMD_REV:
        {
            const uint8_t cmdId = evtParam[0];
            const uint8_t evtParamDataLen = evtParam[1];
            const uint8_t srcEp = evtParam[2]; // command received src ep
            const uint16_t sourceAddr = (uint16_t)evtParam[3] + 
                                        (uint16_t)(evtParam[4] << 8 & 0xff00);

            const uint8_t expecetdEvtParamLen = 4;
            if(expecetdEvtParamLen != evtParamDataLen)
            {
                printf("IASZONE_ENROLL_REQ_CMD_REV Payload Error\r\n");
                return ZB_ERR_INVALID_PARAM;
            } 

            uint16_t zoneType  = evtParam[5] + ((uint16_t)evtParam[6]<<8);
            uint16_t manufCode = evtParam[7] + ((uint16_t)evtParam[8]<<8); 
            printf("IAS ZONE Enroll request received [0x%04x : 0x%x]\r\n"
                   "cmd id is 0x%02x\r\n"
                   "zone type is 0x%04x\r\n"
                   "manufacture code is 0x%04x\r\n",
                   cmdId, sourceAddr, srcEp, zoneType, manufCode);

            struct _zclTxParam apsParam;
            apsParam.dstAddrMode = ZB_APS_ADDR_MODE_SHORT;
            apsParam.dstAddr.shortAddr = sourceAddr;
            apsParam.dstEp = srcEp;
            apsParam.srcEp = ep;
            apsParam.profileId = ZB_PROFILE_ID_HA;
            apsParam.txOptions = ZB_APS_TX_OPTIONS_ACK_TRANS;
            apsParam.radius = 0;  //radius for APSDE-DATA.request primitive, 0 means no limit for radius
            apsParam.disZclDefaultResp = true;
            apsParam.dir = ZCL_CLUST_DIR_IN; // Client to Server 
            apsParam.manufactureSpecific = false;

            zcl_iasZoneSendEnrollResponse(&apsParam, 
                                          IAS_ENROLL_RESPONSE_CODE_SUCCESS,
                                          IAS_ZONE_FIXED_ID,
                                          zcl_IasZoneEnrollRspCb);

            return ZB_SUCC;
        }
        case ZCL_EVT_UPDATE_ATTRIBUTE:
            printf("IAS Zone attribute updated, ep=%d, attribute ID=0x%04x\r\n", ep, *(uint16_t*)evtParam);
            return ZB_SUCC;
        case ZCL_EVT_CLIENT_CMD_RECEIVED:
            printf("IAS Zone client command received is: 0x%02x\r\n", evtParam[0]);
            printf("Payload length: %d\r\n", evtParam[1]);
            for(int i=0; i<evtParam[1]; i++)
            {
                printf("Payload[%d]:0x%x\r\n", i, evtParam[i+2]);
            }
            return ZB_SUCC;
        default:
            break;
    }

    return ZB_ERR_UNSUPPORTED;
}

zbRet_t zcl_touchlinkEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam)
{
    switch(evtId)
    {
        case ZCL_EVT_TL_ON_IDENTIFY:
        {
            uint16_t dur;
            memcpy(&dur, evtParam, 2);
            printf("Identify duration is: %d\r\n", dur);
        }
        break;
        case ZCL_EVT_TL_INI_RESULT:
        {
            printf("Status of result is: 0x%x\r\n", evtParam[0]);
            break;
        }
        default:
        break;
    }

    return ZB_SUCC;    
}

zbRet_t zcl_windowCoveringEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam)
{
    switch(evtId)
    {
        case ZCL_EVT_UPDATE_ATTRIBUTE:
        {
            uint16_t attrId;
            uint8_t data[50];
            uint8_t dataSize = 0;
            memcpy(&attrId, evtParam, 2);
            printf("window covering attribute updated, ep=%d, attribute ID=0x%x\r\n", ep, attrId);
            zcl_getAttr(ZCL_CLUST_WINDOW_COVER, attrId, data, &dataSize, ep);
            printf("dataSize= %d, data:\r\n",dataSize);
            for(int i = 0; i< dataSize; i++)
            {
                printf("%d,", data[i]);
            }
        }
        break;
        case ZCL_EVT_CLIENT_CMD_RECEIVED:
        {
           //Appliction layer need to realize the real movements and update the related attributes during and after the movement.
            printf("Window Covering Command Received is: 0x%x\r\n", evtParam[0]);
            printf("Payload length: %d\r\n", evtParam[1]);
            for(int i=0; i<evtParam[1]; i++)
            {
                printf("Payload[%d]:0x%x\r\n", i, evtParam[i+2]);
            }
        }
        break;
    }

    return ZB_SUCC;    
}

void zcl_touchlinkIniScanRespHandler(uint8_t ep, struct _zclTlScanResp * scanResp[], uint8_t num)
{
    uint64_t targetExtAddr = 0;

    printf("the number of received scan response is %d\r\n", num);
    if(!num)
        return;

    for(int i = 0; i < num; i++)
    {
        memcpy(&targetExtAddr, (uint8_t *)scanResp[i] + sizeof(struct _zclTlScanResp), sizeof(uint64_t));
        printf("target[%d]:0x%llx \r\n", i, targetExtAddr); 
    }
    //for example, take target 0
    zcl_touchlinkIniChooseTarget(ep, 0);
}

#if 0
void zbcli_bdb_local_default_report_cfg(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
    uint8_t para_cnt = PRE_CLI_PARAM_NUM;
    const uint8_t EXPECTED_PARA_CNT = PRE_CLI_PARAM_NUM + 1;
    uint8_t ep;

    if(argc != EXPECTED_PARA_CNT)
    {
        printf("Wrong number of args\r\n");
        
        printf("zb_bdb_local_default_report_cfg  <local ep>\r\n"
                       "ep - <uint8_t>\r\n");
        return;
    }
    else
    {   
        get_uint8_from_string(&argv[para_cnt++], &ep);
        zb_bdb_localDefaultReportCfg(ep);   
    }
}

void zbcli_bdb_set_channel_set(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
    uint32_t ret;
    uint8_t para_cnt = PRE_CLI_PARAM_NUM;
    uint32_t primaryChannelSet;
    uint32_t secondaryChannelSet;
    const uint8_t EXPECTED_PARA_CNT = PRE_CLI_PARAM_NUM + 2;

    if(argc != EXPECTED_PARA_CNT)
    {
        printf("Wrong number of args\r\n");
        printf("zb_bdb_set_channel_set <srcEp>\r\n"
                "primaryChannelSet - <uint32_t>\r\n"
                "secondaryChannelSet - <uint32_t>\r\n");
        return;
    }
    else
    {   
        get_uint32_from_string(&argv[para_cnt++], &primaryChannelSet);
        get_uint32_from_string(&argv[para_cnt++], &secondaryChannelSet);
        ret = zb_bdb_setChannelSet(primaryChannelSet, secondaryChannelSet);
        if(ret)
        {
            printf("%s fails to set bdbChannelSet with error code(0x%lx)\r\n", __func__, ret);
        }
    }
}

void  zb_bdb_appCb(struct _zbBdbCbMsg *cbMsg)
{
    printf("%s: msgType 0x%x, status 0x%x, bindCnt %d\r\n", __func__, cbMsg->msgType, cbMsg->status, cbMsg->bindCnt);    
}

void zbcli_bdb_start_Fb_Initiator(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
    uint8_t para_cnt = PRE_CLI_PARAM_NUM;
    uint8_t srcEp;
    uint8_t commissionMode;
    uint16_t groupId;
    const uint8_t EXPECTED_PARA_CNT = PRE_CLI_PARAM_NUM + 2;

    if(argc != EXPECTED_PARA_CNT)
    {
        printf("Wrong number of args\r\n");
        printf("zb_bdb_start_Fb_Initiator <srcEp,groupId>\r\n"
                "srcEp - <uint8_t>\r\n"
                "groupId - <uint16_t>group id \r\n");
        return;
    }
    else
    {   
        get_uint8_from_string(&argv[para_cnt++], &srcEp);
        get_uint16_from_string(&argv[para_cnt++], &groupId);
        zb_bdb_startFindBindInitiator(srcEp, groupId, zb_bdb_appCb);
    }
}

void zbcli_bdb_fb_start_target(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
    uint8_t para_cnt = PRE_CLI_PARAM_NUM;
    uint8_t ep;
    uint16_t identifyTimeInSec;
    const uint8_t EXPECTED_PARA_CNT = PRE_CLI_PARAM_NUM + 2;

    if(argc != EXPECTED_PARA_CNT)
    {
        printf("Wrong number of args\r\n");
        printf("zb_bdb_fb_target <srcEp, identifyTimeInSec>\r\n"
                "srcEp - <uint8_t>\r\n"
                "identifyTimeInSec - <uint16_t>\r\n");
        return;
    }
    else
    {   
        get_uint8_from_string(&argv[para_cnt++], &ep);
        get_uint16_from_string(&argv[para_cnt++], &identifyTimeInSec);
        zb_bdb_startFindBindTarget(ep, identifyTimeInSec, zb_bdb_appCb);
    }
}

void zbcli_bdb_nwk_form(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
    const uint8_t EXPECTED_PARA_CNT = PRE_CLI_PARAM_NUM;

    if(argc != EXPECTED_PARA_CNT)
    {
        printf("Wrong number of args\r\n");
        printf("zb_bdb_nwk_form <void>\r\n");
        return;
    }
    else
    {   
        zb_bdb_nwkForm(zb_bdb_appCb);
    }
}

void zbcli_bdb_nwk_steer(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv)
{
    const uint8_t EXPECTED_PARA_CNT = PRE_CLI_PARAM_NUM;

    if(argc != EXPECTED_PARA_CNT)
    {
        printf("Wrong number of args\r\n");
        printf("zb_bdb_nwk_steer <void>\r\n");
        return;
    }
    else
    {   
        zb_bdb_nwkSteer(zb_bdb_appCb);
    }
}

#endif

#if defined(CFG_ZIGBEE_CLI)
const static struct cli_command cmds_user[] STATIC_CLI_CMD_ATTRIBUTE = {
    #if 0
    //bdb
    { "zb_bdb_local_default_report_cfg",  "bdb local default report ", zbcli_bdb_local_default_report_cfg},
    { "zb_bdb_set_channel_set",  "bdb set channel set", zbcli_bdb_set_channel_set},
    { "zb_bdb_start_fb_initiator",  "bdb start fb initiator", zbcli_bdb_start_Fb_Initiator},
    { "zb_bdb_start_fb_target",  "bdb start fb target", zbcli_bdb_fb_start_target},
    { "zb_bdb_nwk_form",  "bdb nwk formation", zbcli_bdb_nwk_form},
    { "zb_bdb_nwk_steer",  "bdb nwk steering", zbcli_bdb_nwk_steer},
    #endif
    { "zb_zcl_ota_load_ota_image", "load ota image for client to download", zbcli_zcl_ota_load_ota_image},
    { "zb_zcl_ota_set_image_block_resp", "delay client's next image block request or enable rate limiting", zbcli_zcl_ota_set_image_block_resp},
};
#endif

static void zb_regCommonCb(void)
{
    // register zb event;
    zb_registerCb(zb_eventHandler);

    // register aps indication callback
    //zb_registerApsIndCb(zb_apsIndCb);

    // register aps confirm callback
    //zb_registerApsConfCb(zb_apsConfCb);
}

static void zcl_regClsCb(void)
{
    // register zcl cluster callback
    zcl_registerClustCb(ZCL_CLUST_ONOFF, zcl_onoffEventHandler);
    zcl_registerClustCb(ZCL_CLUST_LEVEL_CTRL, zcl_levelControlEventHandler);
    zcl_registerClustCb(ZCL_CLUST_IDENTIFY, zcl_IdentifyEventHandler);
    zcl_registerClustCb(ZCL_CLUST_COLOR_CTRL, zcl_colorControlEventHandler);
    zcl_registerClustCb(ZCL_CLUST_OTA, zcl_otaEventHandler);
    zcl_registerClustCb(ZCL_CLUST_SCENES, zcl_scenesEventHandler);
    zcl_registerClustCb(ZCL_CLUST_IAS_ZONE, zcl_iasZoneEventHandler);
    zcl_registerClustCb(ZCL_CLUST_TL_COMMS, zcl_touchlinkEventHandler);
    zcl_registerClustCb(ZCL_CLUST_WINDOW_COVER, zcl_windowCoveringEventHandler);

   // zb_manuf_registerCallback();
}

// General Register Zigbee Callbacks
void register_zb_cb(void)
{
    zb_regCommonCb();

    zcl_regClsCb();

    zcl_registerReportCb(zcl_clustReportHandler);

    zcl_touchlinkIniRegScanRespHandler(zcl_touchlinkIniScanRespHandler);
}

void zb_register_app_cb(zb_appCb cb)
{
    appCb = cb;
}

#if (defined(CFG_ZIGBEE_PDS) && defined(CFG_ZIGBEE_SLEEPY_END_DEVICE_STARTUP))
extern bool pds_start;
#endif
void zb_app_startup(void)
{
    //zb_manuf_init();
    uint8_t ep = 1;
    gEp = ep;
    uint16_t profile_id = 0x0104;
    struct _deviceFlags deviceFlags = {0};

#if defined(CFG_ZIGBEE_ROUTER_STARTUP) || defined(CFG_ZIGBEE_NONSLEEPY_END_DEVICE_STARTUP) || defined(CFG_ZIGBEE_COORDINATOR_STARTUP) || defined(CFG_ZIGBEE_SLEEPY_END_DEVICE_STARTUP)

#if defined(CFG_ZIGBEE_ROUTER_STARTUP) || defined(CFG_ZIGBEE_NONSLEEPY_END_DEVICE_STARTUP)
    uint16_t device_id = 0x0102;
    zbRet_t status;

    deviceFlags.touchlinkTarget = 1;
    status = zb_registerDevice(ep, profile_id, device_id, deviceFlags);

    zcl_basicRegisterServer(ep);
    zcl_onOffRegisterServer(ep);
    zcl_identifyRegisterServer(ep);
    zcl_groupsRegisterServer(ep);
    zcl_scenesRegisterServer(ep);
    zcl_levelRegisterServer(ep);
    zcl_colorRegisterServer(ep);
    zcl_touchlinkRegisterServer(ep);
    zcl_touchlinkSetMinRssi(-40);
    zcl_otaRegisterClient(ep);
    zcl_diagnosticsRegisterServer(ep);
#endif

#if defined(CFG_ZIGBEE_COORDINATOR_STARTUP)
    uint16_t device_id = 0x0050;

    zbRet_t status;
    status = zb_registerDevice(ep, profile_id, device_id, deviceFlags);
    
    zcl_basicRegisterClient(ep);
    zcl_identifyRegisterClient(ep);   
    zcl_onOffRegisterClient(ep);
    zcl_levelRegisterClient(ep);
    zcl_scenesRegisterClient(ep);
    zcl_groupsRegisterClient(ep);
    zcl_colorRegisterClient(ep);

    zcl_otaRegisterServer(ep);

    zcl_diagnosticsRegisterClient(ep);
    zcl_powerCfgRegisterClient(ep);
#endif

#if defined(CFG_ZIGBEE_SLEEPY_END_DEVICE_STARTUP)
    uint16_t device_id = 0x0001;
    zbRet_t status;
    status = zb_registerDevice(ep, profile_id, device_id, deviceFlags);
    printf("Dynamic register ep status is 0x%x\r\n", status);
    zcl_basicRegisterServer(ep);
    zcl_powerCfgRegisterServer(ep);
    zcl_pollCtrlRegisterServer(ep);
    //zcl_onOffRegisterServer(ep);
    //zcl_identifyRegisterServer(ep);
    //zcl_windowCoveringRegisterServer(ep);
    //zcl_iasZoneRegisterServer(ep, IAS_ZONE_TYPE_STANDARD_CIE);
    zcl_relHumidityMeasureRegisterServer(ep);
    zcl_tempMeasureRegisterServer(ep);
    #if 0
    zcl_basicRegisterClient(ep);
    zcl_identifyRegisterClient(ep);   
    zcl_onOffRegisterClient(ep);
    zcl_levelRegisterClient(ep);
    zcl_scenesRegisterClient(ep);
    zcl_groupsRegisterClient(ep);
    #endif
    zcl_otaRegisterClient(ep);

    //The default value of longPollInterval is 5s and the default value of shortPollInterval is 500ms in stack
    //set long poll interval to 1s, short poll interval to 500ms
    zcl_pollCtrlSetPollIntvl(ZB_LONG_POLL_INTVL_QUARTER_SECS, ZB_SHORT_POLL_INTVL_QUARTER_SECS);
#endif
    //zb_manuf_registerCluster(ep);

    zcl_otaAutoLocateOtaServerEnable(false);
    zb_initDevices();

    uint32_t currentFileVersion = 0x00000001;
    zcl_setClientAttr(ZCL_CLUST_OTA, ZCL_ATTR_CURRENT_FILE_VERSION, (uint8_t*)&currentFileVersion, 4, ep);
    uint16_t imageType = 0x0000;
    zcl_setClientAttr(ZCL_CLUST_OTA, ZCL_ATTR_DEVICE_IMAGE_TYPE_ID, (uint8_t*)&imageType, 2, ep);

    if(zb_isClustExist(1, ZCL_CLUST_WINDOW_COVER, ZCL_CLUST_DIR_IN))
    {
        //set the type of window covering
        uint8_t type = ZCL_WINDOW_COVERING_TYPE_TILT_BLIND_LIFT_AND_TILT;
        zcl_setAttr(ZCL_CLUST_WINDOW_COVER, ZCL_ATTR_WINDOW_COVERING_TYPE, &type, 1, ep);
        //set the value according to the calibrated value.
        uint16_t value = 0;
        zcl_setAttr(ZCL_CLUST_WINDOW_COVER, ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT, &value, 2, ep);
        zcl_setAttr(ZCL_CLUST_WINDOW_COVER, ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT, &value, 2, ep);
        zcl_setAttr(ZCL_CLUST_WINDOW_COVER, ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT, &value, 2, ep);
        zcl_setAttr(ZCL_CLUST_WINDOW_COVER, ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT, &value, 2, ep);
    }
    
    #if defined(CFG_ZIGBEE_NONSLEEPY_END_DEVICE_STARTUP)
        zb_setRole(ZB_ROLE_NONSLEEPY_ENDDEVICE);
        printf("set role to 0x%x\r\n", ZB_ROLE_NONSLEEPY_ENDDEVICE);
    #elif defined(CFG_ZIGBEE_SLEEPY_END_DEVICE_STARTUP)
        zb_setRole(ZB_ROLE_SLEEPY_ENDDEVICE);
        printf("set role to 0x%x\r\n", ZB_ROLE_SLEEPY_ENDDEVICE);
    #elif defined(CFG_ZIGBEE_ROUTER_STARTUP)
        zb_setRole(ZB_ROLE_ROUTER);
        printf("set role to 0x%x\r\n", ZB_ROLE_ROUTER);
    #else
        zb_setRole(ZB_ROLE_COORDINATOR);
        printf("set role to 0x%x\r\n", ZB_ROLE_COORDINATOR);
        if(!zb_isJoined())
        {
            //for test: using the "zb_form <channel> <PANID>" CLI command to form a nwk manually
            return;
        }
    #endif

    #if (defined(CFG_ZIGBEE_PDS) && defined(CFG_ZIGBEE_SLEEPY_END_DEVICE_STARTUP))
    zb_pds_enable(1);
    #endif
    #if defined(CFG_ZIGBEE_HBN) && defined(CFG_ZIGBEE_SLEEPY_END_DEVICE_STARTUP)
    extern bool isResetSysFromHbnFastboot;
    if(zb_isJoined() && isResetSysFromHbnFastboot)
    {
        isResetSysFromHbnFastboot = false;
        zb_setMacSeqNum(zsedNwkInfo.macSeqNum);
        zb_setNwkSeqNum(zsedNwkInfo.nwkSeqNum);
        zb_setApsCounter(zsedNwkInfo.apsCounter);
        zcl_setTransNum(zsedNwkInfo.zclTransNum);
    }
    #endif
    
    zb_start();

#endif
}

#endif
