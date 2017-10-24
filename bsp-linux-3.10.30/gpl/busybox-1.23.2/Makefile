SUMMARY = "Tiny versions of many common UNIX utilities in a single small executable."
DESCRIPTION = "BusyBox combines tiny versions of many common UNIX utilities into a single small executable. It provides minimalist replacements for most of the utilities you usually find in GNU fileutils, shellutils, etc. The utilities in BusyBox generally have fewer options than their full-featured GNU cousins; however, the options that are included provide the expected functionality and behave very much like their GNU counterparts. BusyBox provides a fairly complete POSIX environment for any small or embedded system."
HOMEPAGE = "http://www.busybox.net"
BUGTRACKER = "https://bugs.busybox.net/"
DEPENDS += "kern-tools-native"

# bzip2 applet in busybox is based on lightly-modified bzip2 source
# the GPL is version 2 only
LICENSE = "GPLv2 & bzip2"

include ../../configs.mk

MODULE_NAME	= busybox-1.23.2
MODULE_FILENAME	= $(MODULE_NAME).tar.bz2
WORKDIR		= files
FEATURE_SUID	= y

CONFIGURE	= ./configure $(DEFAULT_CONF)

#########################################
# Make Rules
#########################################
all: module_decompress module_configure module_make module_install
	
clean: module_clean

module_decompress:
ifeq ($(wildcard $(CURRENT_PATH)/$(MODULE_NAME)), )
	@tar xvf $(PACKAGES_ROOT)/$(MODULE_FILENAME) -C $(CURRENT_PATH)
#	@cp -rf $(CURRENT_PATH)/patches $(CURRENT_PATH)/$(MODULE_NAME)
#	@cd $(CURRENT_PATH)/$(MODULE_NAME);patch -p1  < patches/busybox-1.20.2-fix-resource-h-failure.patch
endif

module_configure:
	cp config_busybox $(MODULE_NAME)/configs/oem_defconfig

module_make:
	@cd $(CURRENT_PATH)/$(MODULE_NAME);make -j $(NCPU) V=1 ARCH=arm CROSS_COMPILE=$(HOST)- SKIP_STRIP=y oem_defconfig
	@cd $(CURRENT_PATH)/$(MODULE_NAME);make -j $(NCPU) V=1 ARCH=arm CROSS_COMPILE=$(HOST)- SKIP_STRIP=y busybox_unstripped
	@cp $(CURRENT_PATH)/$(MODULE_NAME)/busybox_unstripped $(CURRENT_PATH)/$(MODULE_NAME)/busybox

module_install:
#	@cd $(CURRENT_PATH)/$(MODULE_NAME);make -j $(NUM_OF_CORES) V=1 ARCH=arm CROSS_COMPILE=$(HOST)- busybox.links
	@cd $(CURRENT_PATH)/$(MODULE_NAME);make CONFIG_PREFIX=$(LC_INSTALL_PATH) install 
	@cp -arf $(LC_INSTALL_PATH)/* $(GPL_INSTALL_PATH)

module_clean:
ifneq ($(wildcard $(CURRENT_PATH)/$(MODULE_NAME)), )
	@rm -rf $(CURRENT_PATH)/$(MODULE_NAME)
endif
ifneq ($(wildcard $(LC_INSTALL_PATH)), )
	@rm -rf $(LC_INSTALL_PATH)
endif
