#########################################
# Makefile for C-ARES
#########################################
include ../../configs.mk

#########################################
# Define Module Version
#########################################
MODULE_NAME	= ATH6KL-LSDK-WLAN-AIO-SS-5.0.1.42_20160108
MODULE_FILENAME	= $(MODULE_NAME).tgz
BUILD_NAME = WLAN-AIO-SS

#########################################
# Define Path & Compiler
#########################################
CURRENT_PATH		= $(PWD)
export CFLAGS       = $(COPTS) -I$(CURRENT_PATH)/$(MODULE_NAME)/include

export CONFIG_KERNEL_3_10_9=y

#########################################
# Define Configure
#########################################
CONFIGURE	= 	./configure \
                --host=$(HOST) \
                --prefix=$(GPL_INSTALL_PATH) \

#########################################
# Make Rules
#########################################
all: module_decompress module
	
clean: module_clean

module_decompress:
ifeq ($(wildcard $(CURRENT_PATH)/$(BUILD_NAME)), )
	@tar xvf $(CURRENT_PATH)/$(MODULE_FILENAME) -C $(CURRENT_PATH)
	@cp config.ss $(BUILD_NAME)/build/scripts/ss
	@cd $(CURRENT_PATH)/$(BUILD_NAME)/build/;make BOARD_TYPE=ss BUILD_REG_SS_PATCH=y drivers_da_patch
endif

module:
	@cd $(CURRENT_PATH)/$(BUILD_NAME)/build/;make BOARD_TYPE=ss BUILD_REG_SS_PATCH=y CONFIG_KERNEL_3_10_9=y
	@$(STRIP) -g $(CURRENT_PATH)/$(BUILD_NAME)/rootfs-ss.build/lib/modules/*

module_clean:
ifneq ($(wildcard $(CURRENT_PATH)/$(BUILD_NAME)), )
	@rm -rf $(CURRENT_PATH)/$(BUILD_NAME)
endif
