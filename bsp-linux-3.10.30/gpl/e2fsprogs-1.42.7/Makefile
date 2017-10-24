SUMMARY = "Ext2 Filesystem Utilities"
DESCRIPTION = "The Ext2 Filesystem Utilities (e2fsprogs) contain all of the standard utilities for creating, \
fixing, configuring , and debugging ext2 filesystems."
HOMEPAGE = "http://e2fsprogs.sourceforge.net/"

LICENSE = "GPLv2 & LGPLv2 & BSD & MIT"
LICENSE_e2fsprogs-e2fsck = "GPLv2"
LICENSE_e2fsprogs-mke2fs = "GPLv2"
LICENSE_e2fsprogs-fsck = "GPLv2"
LICENSE_e2fsprogs-tune2fs = "GPLv2"
LICENSE_e2fsprogs-badblocks = "GPLv2"

include ../../configs.mk

MODULE_NAME	= e2fsprogs-1.42.7
MODULE_FILENAME	= $(MODULE_NAME).tar.gz

CONFIGURE	= ./configure $(DEFAULT_CONF)\
#--enable-elf-shlibs
#########################################
# Make Rules
#########################################
all: module_decompress module_configure module_make module_install
	
clean: module_clean

module_decompress:
ifeq ($(wildcard $(CURRENT_PATH)/$(MODULE_NAME)), )
	@tar xvf $(PACKAGES_ROOT)/$(MODULE_FILENAME) -C $(CURRENT_PATH)
endif

module_configure:
ifneq ($(wildcard $(CURRENT_PATH)/$(MODULE_NAME)/Makefile), )
	@rm -rf $(CURRENT_PATH)/$(MODULE_NAME)/Makefile
endif
	@cd $(CURRENT_PATH)/$(MODULE_NAME);$(CONFIGURE)

module_make:
	@cd $(CURRENT_PATH)/$(MODULE_NAME);make -j $(NCPU)

module_install:
	@cd $(CURRENT_PATH)/$(MODULE_NAME);make -j $(NCPU) DESTDIR=$(LC_INSTALL_PATH) install
	@cp -arf $(LC_INSTALL_PATH)/$(GPL_INSTALL_PATH)/* $(LC_INSTALL_PATH)/
	@rm -rf $(LC_INSTALL_PATH)/home

	@find $(LC_INSTALL_PATH)/ -name "*.la" | xargs rm -f
	@find $(LC_INSTALL_PATH)/ -name "*.a" | xargs rm -f
	@find $(LC_INSTALL_PATH)/ -name "doc" | xargs rm -rf
	@find $(LC_INSTALL_PATH)/ -name "man" | xargs rm -rf
	@find $(LC_INSTALL_PATH)/ -name "gtk-doc" | xargs rm -rf

	@cp -afr --remove-destination $(LC_INSTALL_PATH)/* $(GPL_INSTALL_PATH)/

module_clean:
ifneq ($(wildcard $(CURRENT_PATH)/$(MODULE_NAME)), )
	@rm -rf $(CURRENT_PATH)/$(MODULE_NAME)
endif
ifneq ($(wildcard $(LC_INSTALL_PATH)), )
	@rm -rf $(LC_INSTALL_PATH)
endif
