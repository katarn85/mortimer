
#########################################################################
# ABSOULTE DIRECTORY
#########################################################################
export NCPU ?= $(shell grep -c processor /proc/cpuinfo)

export TOP_DIR
export PROJECT_BUILD_DIR    =$(TOP_DIR)/build

include $(PROJECT_BUILD_DIR)/project.cfg
export PROJECT_DIR    = $(TOP_DIR)/project/$(PROJECT_NAME)_$(PROJECT_VER)
#########################################################################
# LINK DIRECTORY
#########################################################################
export PROJECT_ROOT   = $(PROJECT_BUILD_DIR)/project
export KERNEL_ROOT    = $(PROJECT_BUILD_DIR)/kernel
export GPL_ROOT       = $(PROJECT_BUILD_DIR)/gpl
export PACKAGES_ROOT  = $(PROJECT_BUILD_DIR)/packages
export TOOLCHAIN_ROOT = $(PROJECT_BUILD_DIR)/toolchain

#########################################################################
# EXPORT DIRECTORY
#########################################################################
export EXPORT_ROOT     = $(PROJECT_BUILD_DIR)/export
export EXPORT_KERNEL   = $(EXPORT_ROOT)/kernel
export EXPORT_GPL      = $(EXPORT_ROOT)/gpl

#########################################################################
# Release DIRECTORY
#########################################################################
export RELEASE_ROOT    = $(PROJECT_BUILD_DIR)/release

#########################################################################
# MISC
#########################################################################
export __CHECK_TOP_DIR  =$(TOP_DIR)
include $(PROJECT_DIR)/project.mk

