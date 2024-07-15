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
CONFIG_ENABLE_BLSYNC:=0

CONFIG_BUILD_ROM_CODE := 1

CONFIG_LINK_CUSTOMER := 1

CONFIG_USE_PSRAM := 1
CONFIG_EASYFLASH_ENABLE := 0
CONFIG_LITTLEFS := 1

CONFIG_BT:=0
ifeq ($(CONFIG_BT),1)
CONFIG_BT_PERIPHERAL:=1
CONFIG_BT_STACK_CLI:=1
endif

# if CONFIG_PDS_CPU_PWROFF is defined, CONFIG_LINK_CUSTOMER must be defined to avoid linking the default .ld file
ifeq ($(CONFIG_PDS_CPU_PWROFF),1)
CONFIG_LINK_CUSTOMER := 1
endif

CONFIG_OPENTHREAD_ENABLE := 1

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

CONFIG_SYS_AOS_CLI_ENABLE := 1

CONFIG_MBEDTLS_BIGNUM_USE_HW := 1
CONFIG_MBEDTLS_AES_USE_HW := 1
CONFIG_MBEDTLS_ECC_USE_HW := 1
CONFIG_MBEDTLS_SHA256_USE_HW := 1

CONF_ENABLE_COREDUMP:=0
CONFIG_OTBR := 1

ifeq ($(CONFIG_USE_WIFI_BR), 1)
CONFIG_USE_DTS_SPI_CONFIG := 1
else
CONFIG_ETHERNET := 1
endif

CONFIG_LWIP_DEBUG := 0
CONFIG_SYS_DMA_ENABLE:=1
CONFIG_CUSTOM_BOARD:=0
CONFIG_CPP_ENABLE :=1

CONFIG_IPV4 :=1
CONFIG_IPV6 :=1
