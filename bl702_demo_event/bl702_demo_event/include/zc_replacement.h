#ifndef __ZC_REPLACE_H__
#define __ZC_REPLACE_H__

#define REPLACED_ZC_ADDR  "replaced_zc_addr"

void zbcli_dump_pan_info(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv);
void zbcli_pan_reset(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv);
void zbcli_pan_startup(char* pcWriteBuffer, int xWriteBufferLen, int argc, char** argv);

void zc_replacement_save_addr(uint64_t ieeeAddr);
void zc_replacement_save_target_panId(uint16_t targetPanId);
void replaced_zc_addr_restore(void);

#endif /* __ZC_REPLACE_H__ */