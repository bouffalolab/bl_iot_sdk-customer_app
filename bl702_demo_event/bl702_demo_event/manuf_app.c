#if defined(CFG_ZIGBEE_ENABLE)
#include <stdint.h>
#include "zcl_common.h"

void zbapp_manuf_init(uint8_t ep) {}

void zbapp_manuf_registerCluster(uint8_t ep) {}

uint8_t zbapp_manuf_clusterEventHandler(uint8_t ep, uint16_t clustId, uint8_t evtId, void * evtParam)
{
    return ZCL_STATUS_SUCCESS;
}
#endif
