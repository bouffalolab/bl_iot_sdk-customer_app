#
#compiler flag config domain
#

#
#board config domain
#


#
#app specific config domain
#

CONFIG_CHIP_NAME := BL702

CONFIG_USE_STDLIB_MALLOC := 0

CONFIG_USB_CDC := 1

CONFIG_BUILD_ROM_CODE := 1
CONFIG_DBG_RUN_ON_FPGA := 0

CONFIG_EASYFLASH_ENABLE:=1
CONFIG_USE_PSRAM := 0

# use internal RC32K by default; may set to 1 for better accuracy if there is XTAL32K on the board
CONFIG_USE_XTAL32K := 0

# disable temperature calibration by default
CONFIG_TCAL := 0

ifeq ($(CONFIG_PDS_ENABLE),1)
CONFIG_CLI_DISABLE := 1
CONFIG_BT_STACK_CLI := 0
CONFIG_PDS_LEVEL ?= 31
ifeq ($(CONFIG_PDS_LEVEL),31)
CONFIG_LINK_CUSTOMER := 1
endif

ifeq ($(CONFIG_BT),1)
# use XTAL32K for ble pds31
CONFIG_USE_XTAL32K := 1
endif
endif

ifeq ($(CONFIG_ZIGBEE), 1)
ifneq ($(CONFIG_ZB_NCP_ENABLE), 1)
CONFIG_ZIGBEE_CLI := 1
endif
endif

ifeq ($(CONFIG_USB_CDC),1)
CONFIG_BL702_USE_USB_DRIVER := 1
endif

CONFIG_BL702_USE_ROM_DRIVER := 1

LOG_ENABLED_COMPONENTS := hosal vfs

CONF_ENABLE_COREDUMP:=1

CONFIG_SYS_COMMON_MAIN_ENABLE := 1
CONFIG_SYS_APP_TASK_STACK_SIZE := 2048
CONFIG_SYS_APP_TASK_PRIORITY := 15
CONFIG_SYS_BLOG_ENABLE := 1

ifneq ($(CONFIG_CLI_DISABLE),1)
CONFIG_SYS_VFS_ENABLE := 1
CONFIG_SYS_VFS_UART_ENABLE := 1
CONFIG_SYS_AOS_CLI_ENABLE := 1
CONFIG_SYS_AOS_LOOP_ENABLE := 1
endif

ifeq ($(CONFIG_ZB_NCP_ENABLE), 1)
CONFIG_SYS_AOS_LOOP_TASK_PRIORITY := 22
endif

CONFIG_MBEDTLS_AES_USE_HW:=1
CONFIG_MBEDTLS_BIGNUM_USE_HW:=1
CONFIG_MBEDTLS_ECC_USE_HW:=1

EXT_CFG_FILE := $(PROJECT_PATH)/proj_config_ext.mk
ifeq ($(EXT_CFG_FILE), $(wildcard $(EXT_CFG_FILE)))
include $(PROJECT_PATH)/proj_config_ext.mk
endif


ifeq ($(CONFIG_ZC_REPLACE_ENABLE), 1)
CONFIG_EASYFLASH_ENABLE:=1
endif