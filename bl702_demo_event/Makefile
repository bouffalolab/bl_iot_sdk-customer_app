#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_PATH := $(abspath .)
PROJECT_NAME :=  $(notdir $(PROJECT_PATH))
PROJECT_BOARD := evb
export PROJECT_PATH PROJECT_BOARD
#CONFIG_TOOLPREFIX :=

BLECONTROLLER_LIBS := m1s1 m0s1 m0s1s m0s1p m16s1 m0s0sp m0s1t10
BLECONTROLLER_LIB_DEFAULT := m1s1
BLECONTROLLER_LIB_PREFIX := blecontroller_702_

ZBSTACK_ADDITION_LIBS := gpp
ZBSTACK_LIB_DEFAULT := zbstack
ZBSTACK_LIB_PREFIX := zbstack_

-include ./proj_config.mk

ifeq ($(origin BL60X_SDK_PATH), undefined)
BL60X_SDK_PATH_GUESS ?= $(shell pwd)
BL60X_SDK_PATH ?= $(BL60X_SDK_PATH_GUESS)/../..
$(info ****** Please SET BL70X_SDK_PATH ******)
$(info ****** Trying SDK PATH [$(BL60X_SDK_PATH)])
endif

INCLUDE_COMPONENTS += bl702 bl702_std newlibc bl702_rf hosal vfs yloop utils cli blog
INCLUDE_COMPONENTS += blmtd blfdt
INCLUDE_COMPONENTS += easyflash4 coredump
INCLUDE_COMPONENTS += lmac154 mfd

ifeq ($(CONFIG_USB_CDC),1)
INCLUDE_COMPONENTS += bl702_usb_cdc
endif

ifeq ($(CONFIG_ZIGBEE), 1)
ifeq ($(CONFIG_ZBSTACK_LIB),all)
	INCLUDE_COMPONENTS += $(ZBSTACK_LIB_DEFAULT)
	INCLUDE_COMPONENTS += $(addprefix $(ZBSTACK_LIB_PREFIX), $(ZBSTACK_ADDITION_LIBS))
else
	ifeq ($(findstring $(CONFIG_ZBSTACK_LIB), $(ZBSTACK_ADDITION_LIBS)),)
		INCLUDE_COMPONENTS += $(ZBSTACK_LIB_DEFAULT)
	else
		INCLUDE_COMPONENTS += $(addprefix $(ZBSTACK_LIB_PREFIX), $(CONFIG_ZBSTACK_LIB))
	endif
endif
INCLUDE_COMPONENTS += zbcli ecdsa macphy154 zcl
ifeq ($(CONFIG_ZB_NCP_ENABLE), 1)
INCLUDE_COMPONENTS += zbncp
else
INCLUDE_COMPONENTS += zbapp
endif
endif

ifeq ($(CONFIG_BT),1)
ifeq ($(CONFIG_BLECONTROLLER_LIB),all)
	INCLUDE_COMPONENTS += $(addprefix $(BLECONTROLLER_LIB_PREFIX), $(BLECONTROLLER_LIBS))
else
	ifeq ($(findstring $(CONFIG_BLECONTROLLER_LIB), $(BLECONTROLLER_LIBS)),)
		INCLUDE_COMPONENTS += $(addprefix $(BLECONTROLLER_LIB_PREFIX), $(BLECONTROLLER_LIB_DEFAULT))
	else
		INCLUDE_COMPONENTS += $(addprefix $(BLECONTROLLER_LIB_PREFIX), $(CONFIG_BLECONTROLLER_LIB))
	endif
endif
INCLUDE_COMPONENTS += blestack
endif

ifeq ($(CONFIG_BT_MESH),1)
INCLUDE_COMPONENTS += blemesh
ifeq ($(CONFIG_BT_MESH_MODEL),1)
INCLUDE_COMPONENTS += blemesh_model
endif
endif

ifeq ($(CONFIG_THREAD), 1)
INCLUDE_COMPONENTS += thread openthread_port openthread_utils_bl702 openthread_br
INCLUDE_COMPONENTS += mbedtls_lts lwip
ifeq ($(CONFIG_LITTLEFS), 1)
    INCLUDE_COMPONENTS += littlefs
else ifeq ($(CONFIG_EASYFLASH_ENABLE), 1)
    INCLUDE_COMPONENTS += easyflash4
endif
endif

ifeq ($(CONFIG_ETHERNET),1)
INCLUDE_COMPONENTS += bl702_ethernet lwip lwip_dhcpd netutils blota
endif

ifeq ($(CONFIG_EFLASH_LOADER),1)
INCLUDE_COMPONENTS += bl702_eflash_loader
endif

ifeq ($(CONFIG_ZIGBEE_PROV),1)
INCLUDE_COMPONENTS += blsync_ble cjson
endif

ifeq ($(CONFIG_GEN_ROM),1)
	INCLUDE_COMPONENTS += bl702_freertos_rom
else
	ifeq ($(CONFIG_BUILD_ROM_CODE),1)
		INCLUDE_COMPONENTS += bl702_freertos
	else
		INCLUDE_COMPONENTS += bl702_freertos_rom
	endif
endif

ifeq ($(CONFIG_AUTO_PTS),1)
INCLUDE_COMPONENTS += bleptstest
endif

INCLUDE_COMPONENTS += $(PROJECT_NAME)

ifeq ($(CONFIG_THREAD),1)
include $(BL60X_SDK_PATH)/components/network/thread/openthread_common.mk
endif

include $(BL60X_SDK_PATH)/make_scripts_riscv/project.mk
ifeq ($(CONFIG_ZIGBEE), 1)
include $(BL60X_SDK_PATH)/components/network/zigbee/zigbee_def.mk
endif

ifeq ($(CONFIG_BLECONTROLLER_LIB),all)
LDFLAGS := $(filter-out $(addprefix -l$(BLECONTROLLER_LIB_PREFIX), $(filter-out $(BLECONTROLLER_LIB_DEFAULT), $(BLECONTROLLER_LIBS))), $(LDFLAGS))
endif

ifeq ($(CONFIG_ZBSTACK_LIB),all)
LDFLAGS := $(filter-out $(addprefix -l$(ZBSTACK_LIB_PREFIX), $(ZBSTACK_ADDITION_LIBS)), $(LDFLAGS))
endif
