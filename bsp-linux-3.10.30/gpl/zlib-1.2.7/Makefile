include ../../configs.mk

MODULE_NAME	= zlib-1.2.7
MODULE_FILENAME	= $(MODULE_NAME).tar.bz2

#export CFLAGS   = $(COPTS) -I$(GPL_INSTALL_PATH)/include
#CPPFLAGS                = $(CPPOPTS)
#export LDFLAGS  = -L. -L/lib -L/usr/lib

CONFIGURE	= ./configure  \
		--prefix=$(GPL_INSTALL_PATH)\
		 --shared

#--libdir=/usr/lib

#########################################
# Make Rules
#########################################
all: module_decompress module_configure module_make
	
clean: module_clean

module_decompress:
ifeq ($(wildcard $(CURRENT_PATH)/$(MODULE_NAME)), )
	tar xvf $(PACKAGES_ROOT)/$(MODULE_FILENAME) -C $(CURRENT_PATH)
endif

module_configure:
ifneq ($(wildcard $(CURRENT_PATH)/$(MODULE_NAME)/Makefile), )
	@rm -rf $(CURRENT_PATH)/$(MODULE_NAME)/Makefile
endif

	@cd $(CURRENT_PATH)/$(MODULE_NAME);$(CONFIGURE)

module_make:
	@cd $(CURRENT_PATH)/$(MODULE_NAME);make -j $(NCPU)
	@cd $(CURRENT_PATH)/$(MODULE_NAME);make DESTDIR=$(LC_INSTALL_PATH) install 
	@mv $(LC_INSTALL_PATH)/$(GPL_INSTALL_PATH)/* $(LC_INSTALL_PATH)
	@rm -rf $(LC_INSTALL_PATH)/home

#	install -d  $(LC_INSTALL_PATH)/lib
#	mv $(LC_INSTALL_PATH)/usr/lib/libz.so.* $(LC_INSTALL_PATH)/lib
#	ln -sf ../../lib/libz.so.1.2.7 $(LC_INSTALL_PATH)/usr/lib/libz.so
	
	@find $(LC_INSTALL_PATH)/ -name "*.la" | xargs rm -f
#	@find $(LC_INSTALL_PATH)/ -name "*.a" | xargs rm -f
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
