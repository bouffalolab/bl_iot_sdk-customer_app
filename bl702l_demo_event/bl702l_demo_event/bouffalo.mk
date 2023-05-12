#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ifeq ($(CONFIG_ZIGBEE), 1)
include $(BL60X_SDK_PATH)/components/network/zigbee/zigbee_common.mk
endif
ifeq ($(CONFIG_BLUETOOTH),1)
include $(BL60X_SDK_PATH)/components/network/ble/ble_common.mk
endif
ifeq ($(CONFIG_OPENTHREAD),1)
include $(BL60X_SDK_PATH)/components/network/thread/openthread_common.mk
endif

EXT_MK := $(PROJECT_PATH)/$(notdir $(PROJECT_PATH))/bouffalo_ext.mk
ifeq ($(EXT_MK), $(wildcard $(EXT_MK)))
include $(PROJECT_PATH)/$(notdir $(PROJECT_PATH))/bouffalo_ext.mk
endif

ifeq ($(CONFIG_LINK_CUSTOMER),1)
LINKER_SCRIPTS := 

COMPONENT_ADD_LDFLAGS += -L $(PROJECT_PATH)/$(notdir $(PROJECT_PATH)) $(addprefix -T ,$(LINKER_SCRIPTS))
##
COMPONENT_ADD_LINKER_DEPS := $(addprefix ,$(LINKER_SCRIPTS))

endif ## CONFIG_LINK_CUSTOMER

ifeq ($(CONFIG_PDS_ENABLE),1)
CONFIG_PDS_LEVEL ?= 31
CPPFLAGS += -DCFG_PDS_ENABLE
CPPFLAGS += -DCFG_PDS_LEVEL=$(CONFIG_PDS_LEVEL)
CPPFLAGS += -DCONFIG_HW_SEC_ENG_DISABLE
ifeq ($(CONFIG_ZIGBEE), 1)
CPPFLAGS += -DCFG_ZIGBEE_PDS
ifeq ($(CONFIG_ZB_SIMPLE_MODE_ENABLE), 1)
CPPFLAGS += -DCFG_ZB_SIMPLE_MODE_ENABLE
endif
endif
endif

ifeq ($(CONFIG_HBN_ENABLE), 1)
CPPFLAGS += -DCONFIG_HW_SEC_ENG_DISABLE
CPPFLAGS += -DCFG_HBN_ENABLE
ifeq ($(CONFIG_ZIGBEE), 1)
CPPFLAGS += -DCFG_ZIGBEE_HBN
endif
endif

ifeq ($(CONFIG_BLE_PERIPHERAL_AUTORUN),1)
CPPFLAGS += -DCFG_BLE_PERIPHERAL_AUTORUN
endif

ifeq ($(CONFIG_RFPHY_CLI),1)
CPPFLAGS += -DCFG_RFPHY_CLI_ENABLE
endif

ifeq ($(CONFIG_ZIGBEE_ROUTER_STARTUP),1)
CPPFLAGS += -DCFG_ZIGBEE_ROUTER_STARTUP
endif

ifeq ($(CONFIG_NONSLEEPY_ZIGBEE_END_DEVICE_STARTUP),1)
CPPFLAGS += -DCFG_ZIGBEE_NONSLEEPY_END_DEVICE_STARTUP
endif

ifeq ($(CONFIG_ZIGBEE_SLEEPY_END_DEVICE_STARTUP),1)
CPPFLAGS += -DCFG_ZIGBEE_SLEEPY_END_DEVICE_STARTUP
endif

ifeq ($(CONFIG_ZIGBEE_COORDINATOR_STARTUP),1)
CPPFLAGS += -DCFG_ZIGBEE_COORDINATOR_STARTUP
endif

ifeq ($(CONFIG_ZIGBEE_CLI), 1)
CPPFLAGS += -DCFG_ZIGBEE_CLI
endif

ifeq ($(CONFIG_USE_PSRAM), 1)
CFLAGS   += -DCFG_USE_PSRAM
CPPFLAGS += -DCFG_USE_PSRAM
endif

ifeq ($(CONFIG_BLE_AUTO_TRIG_SCAN), 1)
CFLAGS   += -DCFG_BLE_AUTO_TRIG_SCAN
CPPFLAGS += -DCFG_BLE_AUTO_TRIG_SCAN
endif

ifeq ($(CONFIG_BLE_AUTO_TRIG_ADV), 1)
CFLAGS   += -DCFG_BLE_AUTO_TRIG_ADV
CPPFLAGS += -DCFG_BLE_AUTO_TRIG_ADV
endif

ifeq ($(CONFIG_ZIGBEE_NO_POLL_AFTER_WAKEUP), 1)
CFLAGS   += -DCFG_ZIGBEE_NO_POLL_AFTER_WAKEUP
CPPFLAGS += -DCFG_ZIGBEE_NO_POLL_AFTER_WAKEUP
endif

ifeq ($(CONFIG_NO_BLE_IF_ZIGBEE_IN_NWK), 1)
CFLAGS   += -DCFG_NO_BLE_IF_ZIGBEE_IN_NWK
CPPFLAGS += -DCFG_NO_BLE_IF_ZIGBEE_IN_NWK
endif

ifeq ($(CONFIG_BLE_DISABLE_ADV_DELAY), 1)
CFLAGS   += -DCFG_BLE_DISABLE_ADV_DELAY
CPPFLAGS += -DCFG_BLE_DISABLE_ADV_DELAY
endif
