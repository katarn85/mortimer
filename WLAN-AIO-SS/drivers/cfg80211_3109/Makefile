NOSTDINC_FLAGS := -I${ATH_DRIVER_TOPDIR}/cfg80211_3109/kernel_80211_h/
obj-$(CONFIG_CFG80211) += cfg80211.o
#obj-$(CONFIG_LIB80211) += lib80211.o
#obj-$(CONFIG_LIB80211_CRYPT_WEP) += lib80211_crypt_wep.o
#obj-$(CONFIG_LIB80211_CRYPT_CCMP) += lib80211_crypt_ccmp.o
#obj-$(CONFIG_LIB80211_CRYPT_TKIP) += lib80211_crypt_tkip.o

#obj-$(CONFIG_WEXT_CORE) += wext-core.o
#obj-$(CONFIG_WEXT_PROC) += wext-proc.o
#obj-$(CONFIG_WEXT_SPY) += wext-spy.o
#obj-$(CONFIG_WEXT_PRIV) += wext-priv.o

cfg80211-y += core.o sysfs.o radiotap.o util.o reg.o scan.o nl80211.o
cfg80211-y += mlme.o ibss.o sme.o chan.o ethtool.o mesh.o ap.o trace.o
cfg80211-$(CONFIG_CFG80211_DEBUGFS) += debugfs.o
cfg80211-$(CONFIG_CFG80211_WEXT) += wext-compat.o wext-sme.o
ifdef BUILD_REG_SS_PATCH
cfg80211-y += regdb.o
else
 cfg80211-$(CONFIG_CFG80211_INTERNAL_REGDB) += regdb.o
endif
 

CFLAGS_trace.o := -I$(src)

ccflags-y += -D__CHECK_ENDIAN__
ifdef CONFIG_NL80211_TESTMODE
ccflags-y += -DCONFIG_NL80211_TESTMODE
endif

ifdef BUILD_REG_SS_PATCH
$(obj)/regdb.c: $(src)/ss_db.txt $(src)/genregdb.awk
	@$(AWK) -f $(src)/genregdb.awk < $< > $@
else
$(obj)/regdb.c: $(src)/db.txt $(src)/genregdb.awk
	@$(AWK) -f $(src)/genregdb.awk < $< > $@
endif

clean-files := regdb.c
