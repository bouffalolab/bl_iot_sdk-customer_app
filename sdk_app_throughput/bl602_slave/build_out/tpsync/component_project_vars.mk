# Automatically generated build file. Do not edit.
COMPONENT_INCLUDES += $(BL60X_SDK_PATH)/components/stage/tpsync $(BL60X_SDK_PATH)/components/stage/tpsync/include $(BL60X_SDK_PATH)/components/stage/tpsync/ramsync_upper $(BL60X_SDK_PATH)/components/stage/tpsync/desc_buf $(BL60X_SDK_PATH)/components/stage/tpsync/ramsync_low $(BL60X_SDK_PATH)/components/stage/tpsync/blmem
COMPONENT_LDFLAGS +=  -L$(BUILD_DIR_BASE)/tpsync -ltpsync 
COMPONENT_LINKER_DEPS += 
COMPONENT_SUBMODULES += 
COMPONENT_LIBRARIES += tpsync
component-tpsync-build: 
