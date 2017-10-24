DATE := $(shell date +%Y.%m.%d\ %T)

include $(TOP_DIR)/configs.mk
include $(PROJECT_DIR)/project.mk

###############################################################################
# ALL
###############################################################################
all: init kernel release

.PHONY: all 

###############################################################################
# init        
###############################################################################

init: check-dir check-toolchain 

.PHONY: init

check-toolchain:
	@if [ ! -e $(TOOLCHAIN_BASE)/$(TOOLCHAIN_DIR) ]; then\
		tar xvf $(TOOLCHAIN_BASE)/$(TOOLCHAIN_FN) -C $(TOOLCHAIN_BASE)/;	\
	fi;

	@echo "make symbolic link ... TOOLCHAIN_ROOT"
	@rm -f $(TOOLCHAIN_ROOT)
	@ln -sf $(TOOLCHAIN_BASE)/$(TOOLCHAIN_DIR) $(TOOLCHAIN_ROOT)

.PHONY: check-toolchain

toolchain-clean:
	rm -rf $(TOOLCHAIN_ROOT)
	rm -rf $(TOOLCHAIN_BASE)/$(TOOLCHAIN_DIR)
	
.PHONY: toolchain-clean

check-dir:
	@echo "##################################################################"
	@echo "# INIT                                                           #"
	@echo "##################################################################"
	@echo ""
	
	@if [ ! $(__CHECK_TOP_DIR)=$(TOP_DIR) ]; then\
		echo "ERROR:: TOP_DIR mismatch. " $(__CHECK_TOP_DIR) " vs " $(TOP_DIR); \
		exit 1; \
	fi;

	@echo "TOP_DIR="$(TOP_DIR)
	@echo "GPL_DIR="$(GPL_DIR)
	@echo "KERNEL_DIR="$(KERNEL_DIR)
	@sleep 1

	@echo "make symbolic link ... PROJECT_ROOT"
	@rm -f $(PROJECT_ROOT)
	@ln -sf $(PROJECT_DIR) $(PROJECT_ROOT)
	
	@echo "make symbolic link ... GPL_ROOT"
	@rm -f $(GPL_ROOT)
	@ln -sf $(GPL_DIR) $(GPL_ROOT)

	@echo "make symbolic link ... KERNEL_ROOT"	
	@rm -f $(KERNEL_ROOT)
	@ln -sf $(KERNEL_DIR) $(KERNEL_ROOT)

	@echo "make symbolic link ... PACKAGES_ROOT"
	@rm -f $(PACKAGES_ROOT)
	@ln -sf $(PACKAGES_DIR) $(PACKAGES_ROOT)

	@echo "make gpl install path"
	@mkdir -p $(GPL_INSTALL_PATH)

	@echo "make export and release path"
	@mkdir -p $(EXPORT_ROOT)
	@mkdir -p $(EXPORT_KERNEL)
	@mkdir -p $(EXPORT_GPL)
	@mkdir -p $(RELEASE_ROOT)
		
.PHONY: check-dir


check-clean:
	@rm -rf $(EXPORT_ROOT)
	@rm -rf $(RELEASE_ROOT) 
	@rm -rf $(PROJECT_ROOT)
	@rm -rf $(GPL_ROOT)
	@rm -rf $(KERNEL_ROOT) 
	@rm -f $(PACKAGES_ROOT)
	@rm -fr $(GPL_INSTALL_PATH)

	@find . -name "*.bak" -print0 | xargs -0 rm -rf
	
.PHONY: check-clean

###############################################################################
# Kernel      
###############################################################################
include $(PROJECT_DIR)/Makefile.kernel

###############################################################################
# GPL Library 
###############################################################################
include $(PROJECT_DIR)/Makefile.gpl

###############################################################################
# Release 
###############################################################################
release: kernel-export 
	
	@echo "##################################################################"
	@echo "# Release Images (generate signature, release note)              #"
	@echo "##################################################################"

	if [ -e $(EXPORT_KERNEL)/images ]; then\
	  cp -afr $(EXPORT_KERNEL)/images/* $(RELEASE_ROOT);\
	fi;		

.PHONY: release

###############################################################################
# CLEAN
###############################################################################
clean: check-dir kernel-clean gpl-clean check-clean

.PHONY: clean

distclean: check-dir kernel-clean gpl-cleanall check-clean toolchain-clean
	
.PHONY: distclean

