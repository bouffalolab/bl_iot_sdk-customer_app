#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := demo_master
PROJECT_PATH := $(abspath .)
PROJECT_BOARD := evb
export PROJECT_PATH PROJECT_BOARD
#CONFIG_TOOLPREFIX :=

-include ./proj_config.mk

ifeq ($(origin BL60X_SDK_PATH), undefined)
BL60X_SDK_PATH_GUESS ?= $(shell pwd)
BL60X_SDK_PATH ?= $(BL60X_SDK_PATH_GUESS)/../../..
$(info ****** Please SET BL70X_SDK_PATH ******)
$(info ****** Trying SDK PATH [$(BL60X_SDK_PATH)])
endif

INCLUDE_COMPONENTS += bl702 bl702_std bl702_rf hosal vfs yloop utils cli blog newlibc
INCLUDE_COMPONENTS += blmtd blfdt lwip netutils 
INCLUDE_COMPONENTS += easyflash4
INCLUDE_COMPONENTS += rfapi tpsync

ifeq ($(CONFIG_VIRT_NET_ENABLE),1)
INCLUDE_COMPONENTS += virt_net
endif

ifeq ($(CONFIG_USB_CDC),1)
INCLUDE_COMPONENTS += bl702_usb_cdc
endif

ifeq ($(CONFIG_ZIGBEE), 1)
INCLUDE_COMPONENTS += zbstack zbcli
endif

ifeq ($(CONFIG_BT),1)
INCLUDE_COMPONENTS += blecontroller blestack
endif

ifeq ($(CONFIG_BT_MESH),1)
INCLUDE_COMPONENTS += blemesh
endif

ifeq ($(CONFIG_ETHERNET),1)
INCLUDE_COMPONENTS += bl702_ethernet lwip lwip_dhcpd netutils blota
endif

ifeq ($(CONFIG_BT_MESH_MODEL),1)
INCLUDE_COMPONENTS += blemesh_model
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


INCLUDE_COMPONENTS += $(PROJECT_NAME)
include $(BL60X_SDK_PATH)/make_scripts_riscv/project.mk
