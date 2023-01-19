####
CONFIG_SYS_VFS_ENABLE:=1
CONFIG_SYS_VFS_UART_ENABLE:=1
CONFIG_SYS_AOS_CLI_ENABLE:=1
CONFIG_SYS_AOS_LOOP_ENABLE:=1
CONFIG_SYS_BLOG_ENABLE:=1
CONFIG_SYS_DMA_ENABLE:=1
CONFIG_SYS_USER_VFS_ROMFS_ENABLE:=0
CONFIG_SYS_APP_TASK_STACK_SIZE:=4096
CONFIG_SYS_APP_TASK_PRIORITY:=15

ifeq ("$(CONFIG_CHIP_NAME)", "BL702")
CONFIG_SYS_COMMON_MAIN_ENABLE:=1
CONFIG_BL702_USE_ROM_DRIVER:=1
CONFIG_BUILD_ROM_CODE := 1
CONFIG_USE_XTAL32K:=1
else ifeq ("$(CONFIG_CHIP_NAME)", "BL702L")
CONFIG_SYS_COMMON_MAIN_ENABLE:=1
CONFIG_BL702_USE_ROM_DRIVER:=1
CONFIG_BUILD_ROM_CODE := 0
CONFIG_USE_XTAL32K:=0
else ifeq ("$(CONFIG_CHIP_NAME)", "BL602")
CONFIG_BL602_USE_ROM_DRIVER:=1
CONFIG_LINK_ROM=1
CONFIG_FREERTOS_TICKLESS_MODE:=0
CONFIG_WIFI:=0
endif

LOG_ENABLED_COMPONENTS:= blog_testc hosal demo_pwm

