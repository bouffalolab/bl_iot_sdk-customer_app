#
#compiler flag config domain
#

#
#board config domain
#


#
#app specific config domain
#

CONFIG_CHIP_NAME := BL702L
CONFIG_CPP_ENABLE :=1
CONFIG_CACHE_SIZE := 16384

CONFIG_USE_STDLIB_MALLOC := 0

CONFIG_BUILD_ROM_CODE := 0
CONFIG_DBG_RUN_ON_FPGA := 0

CONFIG_EASYFLASH_ENABLE := 0
CONFIG_LITTLEFS := 1
CONFIG_USE_PSRAM := 0

# use internal RC32K by default; may set to 1 for better accuracy if there is XTAL32K on the board
CONFIG_USE_XTAL32K := 1 

# disable temperature calibration by default; you can enable it if you have capcode burned in efuse
CONFIG_TCAL := 0

ifeq ($(CONFIG_PDS_ENABLE),1)
CONFIG_PDS_LEVEL ?= 31
ifeq ($(CONFIG_PDS_LEVEL),31)
CONFIG_PDS_CPU_PWROFF := 1
endif
endif

# if CONFIG_PDS_CPU_PWROFF is defined, CONFIG_LINK_CUSTOMER must be defined to avoid linking the default .ld file
ifeq ($(CONFIG_PDS_CPU_PWROFF),1)
#CONFIG_LINK_CUSTOMER := 1
endif

CONFIG_BL702_USE_ROM_DRIVER := 1

LOG_ENABLED_COMPONENTS := hosal vfs

CONF_ENABLE_COREDUMP := 1

CONFIG_SYS_COMMON_MAIN_ENABLE := 1
CONFIG_SYS_APP_TASK_STACK_SIZE := 2048
CONFIG_SYS_APP_TASK_PRIORITY := 14
CONFIG_SYS_BLOG_ENABLE := 1

ifneq ($(CONFIG_PDS_ENABLE),1)
CONFIG_SYS_VFS_ENABLE := 1
CONFIG_SYS_VFS_UART_ENABLE := 1
CONFIG_SYS_AOS_LOOP_ENABLE := 1
ifdef CONFIG_PREFIX
CONFIG_SYS_AOS_CLI_ENABLE := 1
endif
endif

EXT_CFG_FILE := $(PROJECT_PATH)/proj_config_ext.mk
ifeq ($(EXT_CFG_FILE), $(wildcard $(EXT_CFG_FILE)))
include $(PROJECT_PATH)/proj_config_ext.mk
endif

CONFIG_DATA_POLL_CSMA := 1 
CONFIG_THREAD := 1

CONFIG_MBEDTLS_BIGNUM_USE_HW := 1
CONFIG_MBEDTLS_AES_USE_HW := 1
CONFIG_MBEDTLS_ECC_USE_HW := 1
CONFIG_MBEDTLS_SHA256_USE_HW := 1
