#########################################################################
# MODEL VERSION
#########################################################################
#RF=refr
export MODEL_PRODUCT_LINE   = RF

#T:Development,G:Gang
export MODEL_MANUFAC_TYPE   = T

#Project Name
export MODEL_PROJECT        = FAMHUB

#target pcb version: 1~
export MODEL_BOARD          = 7

#US, KR, EU
export MODEL_COUNTRY        = US

#VD SOC HAWKM (SDP1406)
export MODEL_AP             = HAWKM

#PI(0100),DV(0200),PV(0300),PR(1000)
export MODEL_STEP           = 0300
export MODEL_CODE           = $(MODEL_PRODUCT_LINE)$(MODEL_MANUFAC_TYPE)-$(MODEL_PROJECT)$(MODEL_BOARD)$(MODEL_COUNTRY)$(MODEL_AP)-$(MODEL_STEP)

#BUILD MODE : RELEASE | DEBUG
export BUILD_MODE           = RELEASE

#########################################################################
# ROOT DIRECTORY VERSION
#########################################################################
export KERNEL_VER       = 3.10.30
export GPL_VER          = 

#########################################################################
# ROOT DIRECTORY
#########################################################################
export KERNEL_DIR	    = $(TOP_DIR)/kernel/linux-$(KERNEL_VER)
export GPL_DIR          = $(TOP_DIR)/gpl
export PACKAGES_DIR     = $(TOP_DIR)/packages
export RELEASE_DIR	    = $(RELEASE_ROOT)

#########################################################################
# KERNEL OPTIONS
#########################################################################
export KERNEL_DEFCONFIG_FILE = $(MODEL_PROJECT)_$(MODEL_BOARD)_defconfig
export KERNEL_DTS_FILE = SDP1406_$(MODEL_PROJECT)_$(MODEL_BOARD).dts
export KERNEL_DTB_FILE = SDP1406_$(MODEL_PROJECT)_$(MODEL_BOARD).dtb

#########################################################################
# BUILD TOOLS OPTIONS
#########################################################################
export HOST             = armv7l-tizen-linux-gnueabi
export TOOLCHAIN_BASE   = $(TOP_DIR)/toolchain
export TOOLCHAIN_DIR    = armv7l-tizen-20140701
export TOOLCHAIN_FN     = armv7l-tizen-20140701.tar.bz2

#########################################################################
# GPL BUILD DIRECTORY
#########################################################################
CURRENT_PATH    = $(shell pwd)
LC_PATH         = _install_target
LC_INSTALL_PATH = $(CURRENT_PATH)/$(LC_PATH)
export GPL_INSTALL_PATH  = $(EXPORT_GPL)/$(LC_PATH)

#########################################################################
# TOOLCHAIN
#########################################################################
export ARCH                 = arm
export CROSS_COMPILE        = $(HOST)-
export CROSS                = $(CROSS_COMPILE)
export AS                   = $(CROSS_COMPILE)as
export LD                   = $(CROSS_COMPILE)ld
export CC                   = $(CROSS_COMPILE)gcc
export CXX                  = $(CROSS_COMPILE)g++
export AR                   = $(CROSS_COMPILE)ar
export STRIP                = $(CROSS_COMPILE)strip
export OBJCOPY              = $(CROSS_COMPILE)objcopy
export OBJDUMP              = $(CROSS_COMPILE)objdump
export RANLIB               = $(CROSS_COMPILE)ranlib
export NM                   = $(CROSS_COMPILE)nm

#########################################################################
# BUILD OPTIONS  
#########################################################################
export OPTS        := -Wall -fPIC -O2 -mfpu=vfp3 -mfloat-abi=softfp -mcpu=cortex-a12 -mtune=cortex-a12
export COPTS       := $(OPTS)
export CPPOPTS     := $(OPTS)
export CFLAGS      := $(COPTS)
export CPPFLAGS    := $(CPPOPTS)
export AFLAGS      :=
#export ARFLAGS    := crv
export LDFLAGS     :=
export LIBRARY     := 
export CFLAGS      := $(COPTS) -I$(GPL_INSTALL_PATH)/include
export CPPFLAGS    := $(CPPOPTS) -I$(GPL_INSTALL_PATH)/include
export LDFLAGS     := -L$(GPL_INSTALL_PATH)/lib

export PKG_CONFIG_LIBDIR	= $(GPL_INSTALL_PATH)/lib/pkgconfig
export PKG_CONFIG_PATH 		= $(GPL_INSTALL_PATH)/lib/pkgconfig

export DEFAULT_CONF = \
	--host=$(HOST) \
	--target=$(HOST) \
	--prefix=$(GPL_INSTALL_PATH) \
	--disable-silent-rules \
	--disable-dependency-tracking

##############################################################
# include open source list for each project
##############################################################
include $(PROJECT_DIR)/opensource.lst

#########################################################################
# SYSTEM Configs
#########################################################################
export SERIAL_CONSOLE = "115200 ttyS0"
export ROOT_HOME = /home/root
export APPS_HOME = /home/app

