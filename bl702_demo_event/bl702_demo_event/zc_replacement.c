#ifdef CFG_ZC_REPLACE_ENABLE

#include <zb_common.h>
#include <zb_mac.h>
#include <zb_aps.h>
#include <zb_nwk.h>
#include <easyflash.h>
#include <cli.h>
#include <bl_wireless.h>
#include "zc_replacement.h"

static uint16_t targetPanIdToForm = 0xFFFF;

static const struct cli_command cmds_user[] STATIC_CLI_CMD_ATTRIBUTE = {
    { "zb_dump_pan_info", "dump all PAN and TC informations that should be saved during ZC cloneing process", zbcli_dump_pan_info},
    { "zb_pan_startup", "startup restored PAN", zbcli_pan_startup},
    { "zb_pan_reset", "reset to factory default", zbcli_pan_reset},
};

/*  This function is used to retrieve and print various nwk information about the PAN and TC,
    all these information should be set into the clone zc.

    NOTICE: don't change the print format, it's used by the Restore_PAN.py .
 */
void zbcli_dump_pan_info(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv){
    uint64_t macAddr = 0;
    zb_getIeeeAddr((uint8_t *)&macAddr);
    printf("ieee_address: 0X%llX\r\n", macAddr) ;

    uint64_t extPandId = zb_getExtPanId();
    printf("extended_pan_id: 0X%llX\r\n", extPandId);

    uint16_t panId = zb_getPanId();
    printf("pan_id: 0X%04X\r\n", panId);

    uint8_t channel = zb_getChannel();
    printf("radio_channel: %d\r\n", channel);
    
    uint8_t global_tc_link_key[16] = {0};
    uint32_t global_tc_outgoing_frame_counter = 0;
    
    uint8_t nwk_key[16] = {0};
    uint32_t nwk_outgoing_frame_counter = 0;
    uint8_t nwk_key_seq_num = 0;   

    uint16_t link_key_table_size = 0;
    struct _zbApsSecurityMaterial link_key_struct;

    zb_getGlobalTrustCenterLinkKey(global_tc_link_key, &global_tc_outgoing_frame_counter);
    printf("global_key: ");
    for(int i = 0; i < 16; i++){
        printf("%02X", global_tc_link_key[i]);
    }
    printf("\r\n");

    printf("global_fc: %lu\r\n", global_tc_outgoing_frame_counter);
    
    zb_getActiveNwkKey(nwk_key, &nwk_outgoing_frame_counter, &nwk_key_seq_num);
    printf("network_key: ");
    for(int i = 0; i < 16; i++){
        printf("%02X", nwk_key[i]);
    }
    printf("\r\n");
    printf("network_fc: %lu\r\n", nwk_outgoing_frame_counter);
    printf("network_sequence_number: %02X\r\n", nwk_key_seq_num);

    link_key_table_size = zb_getLinkKeyTableSize();
    printf("tc_link_key_number: %d\r\n", link_key_table_size);

    uint32_t abKey_length = sizeof(link_key_struct.abKey);
    uint8_t *ptr_link_key = (uint8_t *)(&link_key_struct);

    for(int i = 0; i < link_key_table_size; i++){   
        
        zb_getLinkKeyTableEntry(&link_key_struct, i);

        printf("tc_link_key[%d]: ", i);
        for(int j = 0; j < abKey_length; j++){
            printf("%02X", link_key_struct.abKey[j]);    
        }

        printf("  0X%llX\r\n", link_key_struct.deviceIeeeAddr);
    }
}

static bool zb_networkFoundCb(struct _zbPanDescriptor panDescriptor)
{

	//current implementation doesn't allow PANID confilict during nwk 	formation, so add work-around here to filter out
	//beacons from non-coordinator devices.
	if(targetPanIdToForm != 0xFFFF && panDescriptor.panId == targetPanIdToForm 		&& !panDescriptor.beaconSuperFrame.bf.isPANCoordinator )
	{
		return false;
	}

	return true;
}


void zbcli_pan_startup(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv){
    
    zb_registerNetworkFoundCb(zb_networkFoundCb);

    zb_start();
}


void zbcli_pan_reset(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv){
    zb_resetToFactoryDefault();

}

int zc_replacement_cli_register(void)
{
    return 0;
}


void zc_replacement_save_addr(uint64_t ieeeAddr)
{
    ef_set_env_blob(REPLACED_ZC_ADDR, &ieeeAddr, sizeof(uint64_t));
}

void zc_replacement_save_target_panId(uint16_t targetPanId)
{
    targetPanIdToForm = targetPanId;
}

void replaced_zc_addr_restore(void)
{
    size_t getLen = 0;
    uint64_t replaced_zc_addr = 0;
    ef_get_env_blob(REPLACED_ZC_ADDR, &replaced_zc_addr, sizeof(replaced_zc_addr), &getLen);

    if(getLen == sizeof(replaced_zc_addr))
    {
        bl_wireless_mac_addr_set(&replaced_zc_addr);
    }
}
#endif