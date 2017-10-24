#
# Please modify KERNELPATH, KERNELARCH and TOOLPREFIX to meet your environment
#

# SS example ###########################################################################################################

export KERNELPATH=$(KERNEL_ROOT)
export TOOLPREFIX=$(CROSS_COMPILE)
export KERNELARCH=arm

# Use local libnl library or not
export BUILD_LIBNL=n
export INCLUDE_LIBNL11_HEADER=${ATH_TOPDIR}/apps/libnl-1/include
export INCLUDE_LIBNL11_LIB=-lm ${ATH_TOPDIR}/apps/libnl-1/lib/libnl.a 

#export PATH:=${TOOLCHAIN}:${PATH}

# Build regdb into cfg80211.ko
export CONFIG_CFG80211_INTERNAL_REGDB=y

# custome behavior
export CONFIG_CE_2_SUPPORT=y

# Direct Audio
export CONFIG_DIRECT_AUDIO_SUPPORT=y
export USB_CUSTOMIZE_SUSPEND=y

# Select Board Data.
export BUILD_BDATA_DB7=y
#export BUILD_BDATA_XPA=y
#export BUILD_BDATA_XPA_DUAL=y
export CONFIG_SWOW_SUPPORT=y
