DESCRIPTION = "Alsa sound library"
HOMEPAGE = "http://www.alsa-project.org"
BUGTRACKER = "https://bugtrack.alsa-project.org/alsa-bug/login_page.php"
SECTION = "libs/multimedia"
LICENSE = "LGPLv2.1 & GPLv2+"

include ../../configs.mk

MODULE_NAME	= alsa-lib-1.0.26
MODULE_FILENAME	= $(MODULE_NAME).tar.bz2

CONFIGURE	=  ./configure $(DEFAULT_CONF) \
			--with-cards=pdaudiocf \
			--disable-python


#########################################
# Make Rules
#########################################
all: module_decompress module_configure module_make module_install
	
clean: module_clean

module_decompress:
ifeq ($(wildcard $(CURRENT_PATH)/$(MODULE_NAME)), )
	@tar xvf $(PACKAGES_ROOT)/$(MODULE_FILENAME) -C $(CURRENT_PATH)
#	@patch -p0 < $(MODULE_NAME).patch
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
	@mv $(LC_INSTALL_PATH)/$(GPL_INSTALL_PATH)/* $(LC_INSTALL_PATH)
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

