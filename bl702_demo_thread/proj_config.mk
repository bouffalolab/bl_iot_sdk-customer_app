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
CONFIG_CPP_ENABLE :=1

CONFIG_CACHE_SIZE := 16384
CONFIG_USE_STDLIB_MALLOC := 0

CONFIG_BUILD_ROM_CODE := 1

CONFIG_LINK_CUSTOMER := 0

CONFIG_USE_PSRAM := 0 
CONFIG_EASYFLASH_ENABLE := 0
CONFIG_LITTLEFS := 1

# if CONFIG_PDS_CPU_PWROFF is defined, CONFIG_LINK_CUSTOMER must be defined to avoid linking the default .ld file
ifeq ($(CONFIG_PDS_CPU_PWROFF),1)
CONFIG_LINK_CUSTOMER := 1
endif

CONFIG_OPENTHREAD_CLI_ENABLE := 1

ifeq ($(CONFIG_USB_CDC),1)
CONFIG_BL702_USE_USB_DRIVER := 1
endif

CONFIG_BL702_USE_ROM_DRIVER := 1

CONFIG_SYS_COMMON_MAIN_ENABLE := 1
CONFIG_SYS_APP_TASK_STACK_SIZE := 2048
CONFIG_SYS_APP_TASK_PRIORITY := 15
CONFIG_SYS_VFS_ENABLE := 1
CONFIG_SYS_AOS_LOOP_ENABLE := 1
CONFIG_SYS_VFS_UART_ENABLE := 1
CONFIG_SYS_BLOG_ENABLE := 0

ifdef CONFIG_PREFIX
CONFIG_SYS_AOS_CLI_ENABLE := 1
endif

CONFIG_MBEDTLS_BIGNUM_USE_HW := 1
CONFIG_MBEDTLS_AES_USE_HW := 1
CONFIG_MBEDTLS_ECC_USE_HW := 1
CONFIG_MBEDTLS_SHA256_USE_HW := 1

CONF_ENABLE_COREDUMP:=1
CONFIG_COMPONENT_BUGKILLER_ENABLE:=1