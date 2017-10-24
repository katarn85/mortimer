# Build list

targetList := drivers_firmware drivers_firmware_transfer drivers
targetList += rootfs_build 

default: ${targetList}

#################################################################################################################################
export SIGMA_TOPDIR=${ATH_TOPDIR}/apps/sigma-dut-intel-rtsplib/DirectDisplay2
export WLAN_DRIVER_TOPDIR=$(ATH_TOPDIR)/drivers

#################################################################################################################################
#
# driver build
#
drivers_patch:
	@echo Build drivers patch
	@echo 00-remore_ns-type.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/00-remore_ns-type.patch
	@echo 01-add_CFG80211_WEXT.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/01-add_CFG80211_WEXT.patch
	@echo 02-remore_netlink-seq.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/02-remore_netlink-seq.patch
	@echo 03-change_IFF_BRIDGE_PORT.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/03-change_IFF_BRIDGE_PORT.patch
	@echo 05-change_fw-request-SB.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/05-change_fw-request-SS.patch
	@echo 06-Kbuild.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/06-Kbuild.patch
	@echo 10-remove_unaligned_copy.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/10-remove_unaligned_copy.patch
	@echo 12-intra_reg.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/12-intra_reg.patch
	@echo 14-enlarge_rx_buffer_size.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/14-enlarge_rx_buffer_size.patch
	@echo 16-enable_bundle_recv.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/16-enable_bundle_recv.patch
	@echo 17-disable_power_save.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/17-disable_power_save.patch
	@echo 19-add_txpower_in_dbgfs.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/19-add_txpower_in_dbgfs.patch
	@echo 21-undef_CONFIG_ANDROID.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/21-undef_CONFIG_ANDROID.patch
	@echo 22-add_p2p0_phy_info.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/22-add_p2p0_phy_info.patch
	@echo 23-add_ss_pid.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/23-add_ss_pid.patch
	@echo 27-scan_timeout_20sec.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/27-scan_timeout_20sec.patch
	@echo 31-recovery_enable.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/31-recovery_enable.patch
	@echo 33-passive_channel_skip.patch
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p0 < patches/33-passive_channel_skip.patch
	@echo 81_diff_Wempty-body
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p2 < patches/81_diff_Wempty-body
	@echo 82_diff_Wmissing-field-initializers
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p2 < patches/82_diff_Wmissing-field-initializers
	@echo 83_diff_Wmissing-prototypes
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p2 < patches/83_diff_Wmissing-prototypes
	@echo 84_diff_Wunused-but-set-variable
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p2 < patches/84_diff_Wunused-but-set-variable
#	@echo 40-remove_CONFIG_PM.patch
#	@cd ${WLAN_DRIVER_TOPDIR}/../ && patch -p1 < ${WLAN_DRIVER_TOPDIR}/patches/40-remove_CONFIG_PM.patch

drivers_da_patch:
	@echo 100-SS_DA_kernel_crash_and_reset_api.patch in Makefile.ss
	@cd ${WLAN_DRIVER_TOPDIR} && patch -p2 < patches/100-SS_DA_kernel_crash_and_reset_api.patch

#
# Sigma package
#
sigmadut_build: rootfs_prep sigmadut_clean
	@echo Build Sigma
	${MAKEARCH} -C $(SIGMA_TOPDIR)/wfd CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld CXX=$(TOOLPREFIX)g++ && \
	cp $(SIGMA_TOPDIR)/wfd/debug/libqcawfd.so $(INSTALL_ROOT)/lib && \
	${MAKEARCH} -C $(SIGMA_TOPDIR)/sigma-dut CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld CXX=$(TOOLPREFIX)g++ && \
	cp $(SIGMA_TOPDIR)/sigma-dut/debug/sigma_dut $(INSTALL_ROOT)/sbin

sigmadut_clean:
	@echo Clean Sigma
	${MAKEARCH} -C $(SIGMA_TOPDIR)/wfd clean && \
	${MAKEARCH} -C $(SIGMA_TOPDIR)/sigma-dut clean
	
