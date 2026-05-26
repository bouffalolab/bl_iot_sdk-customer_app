#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
include $(BL60X_SDK_PATH)/components/network/thread/openthread_common.mk

ifeq ($(CONFIG_BT),1)
include $(BL60X_SDK_PATH)/components/network/ble/ble_common.mk
endif

ifeq ($(CONFIG_SYS_AOS_CLI_ENABLE),1)
CPPFLAGS += -DSYS_AOS_CLI_ENABLE
endif