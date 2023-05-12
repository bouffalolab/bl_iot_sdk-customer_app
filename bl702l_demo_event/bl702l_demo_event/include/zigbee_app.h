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
#ifndef  __ZB_NOTIFY__
#define  __ZB_NOTIFY__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "zb_common.h"
#include "zcl_common.h"

#define CODE_USER_ZB_OTA_SERVER_SEARCH      1
#define CODE_USER_ZB_OTA_QUERY_NEXT_IMGE    2
#define ZB_APP_ZCL_REPORT_CNT_MAX 5

#if defined(CFG_ZIGBEE_HBN)
struct _zclReportInfo{
    uint16_t clustId;
    struct _zclReportCfgRec record;
    uint64_t nextReportBaseCnt;
    uint64_t nextReportDeltaMs;
};
#endif

zbRet_t zb_eventHandler(uint8_t evtId, uint8_t * evtParam);

zbRet_t zcl_onoffEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam);
zbRet_t zcl_levelControlEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam);
zbRet_t zcl_IdentifyEventHandler(uint8_t ep, uint8_t evtId, uint8_t * evtParam);


void register_zb_cb(void);
typedef void (*zb_appCb)(const uint8_t status, const uint8_t roleType);
void zb_register_app_cb(zb_appCb cb);

void zb_app_startup(void);
bool zb_isDevActiveInNwk(void);
void zb_searchOtaServer(void);
void zcl_otaQueryNextImage(void);
#if defined(CFG_ZIGBEE_HBN)
bool zcl_checkIfTriggerAttrReport(bool bReport);
#endif

//placeholder for manufacture to do initialization
void zb_manuf_init();
//placeholder for manufacture to register required clusters
void zb_manuf_registerCluster(uint8_t ep);
//placeholder for manufacture to register required callbacks
void zb_manuf_registerCallback();
#endif // __ZB_NOTIFY__
