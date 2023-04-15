/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#if 0
	{feature_ped_exception,"/system/bin/run-as"},	/* DEFAULT */
	{feature_ped_exception,"/system/bin/dumpstate"},	/* DEFAULT */
	{feature_safeplace_path,"/init"},
	{feature_safeplace_path,"/system/bin/init"},
	{feature_safeplace_path,"/system/bin/app_process32"},
	{feature_safeplace_path,"/system/bin/app_process64"},
	{feature_safeplace_path,"/system/bin/blkid"},
	{feature_safeplace_path,"/system/bin/clatd"},
	{feature_safeplace_path,"/system/bin/cmd"},
	{feature_safeplace_path,"/system/bin/corehelper.sh"},
	{feature_safeplace_path,"/system/bin/crash_dump32"},
	{feature_safeplace_path,"/system/bin/crash_dump64"},
	{feature_safeplace_path,"/system/bin/debuggerd"},
	{feature_safeplace_path,"/system/bin/dnsmasq"},
	{feature_safeplace_path,"/system/bin/dsms"},
	{feature_safeplace_path,"/system/bin/dumpstate"},
	{feature_safeplace_path,"/system/bin/fsck.vfat"},
	{feature_safeplace_path,"/system/bin/fsck.exfat"},
	{feature_safeplace_path,"/system/bin/gatekeeperd"},
	{feature_safeplace_path,"/system/bin/healthd"},
	{feature_safeplace_path,"/system/bin/installd"},
	{feature_safeplace_path,"/system/bin/iod"},
	{feature_safeplace_path,"/system/bin/ip"},
	{feature_safeplace_path,"/system/bin/iptables"},
	{feature_safeplace_path,"/system/bin/iptables-restore"},
	{feature_safeplace_path,"/system/bin/ip6tables"},
	{feature_safeplace_path,"/system/bin/ip6tables-restore"},
	{feature_safeplace_path,"/system/bin/lmkd"},
	{feature_safeplace_path,"/system/bin/lshal"},
	{feature_safeplace_path,"/system/bin/mdf_fota"},
	{feature_safeplace_path,"/system/bin/mkfs.vfat"},
	{feature_safeplace_path,"/system/bin/mkfs.exfat"},
	{feature_safeplace_path,"/system/bin/netd"},
	{feature_safeplace_path,"/system/bin/nst"},
	{feature_safeplace_path,"/system/bin/perfmond"},
	{feature_safeplace_path,"/system/bin/perfprofd"},
	{feature_safeplace_path,"/system/bin/sgdisk"},
	{feature_safeplace_path,"/system/bin/sh"},
	{feature_safeplace_path,"/system/bin/ss"},
	{feature_safeplace_path,"/system/bin/storaged"},
	{feature_safeplace_path,"/system/bin/tc"},
	{feature_safeplace_path,"/system/bin/uncrypt"},
	{feature_safeplace_path,"/system/bin/vold"},
	{feature_safeplace_path,"/system/bin/webview_zygote32"},
	{feature_safeplace_path,"/system/bin/grep"},
	{feature_safeplace_path,"/system/bin/e2fsck"},
	{feature_safeplace_path,"/system/bin/scs"},
	{feature_safeplace_path,"/system/bin/vdc"},
	{feature_safeplace_path,"/system/bin/vaultkeeperd"},
	{feature_safeplace_path,"/system/bin/prepare_param.sh"},
	{feature_safeplace_path,"/system/bin/smdexe"},
	{feature_safeplace_path,"/system/bin/diagexe"},
	{feature_safeplace_path,"/system/bin/ddexe"},
	{feature_safeplace_path,"/system/bin/connfwexe"},
	{feature_safeplace_path,"/system/bin/at_distributor"},
	{feature_safeplace_path,"/system/bin/sdcard"},
	{feature_safeplace_path,"/system/bin/resetreason"},
	{feature_safeplace_path,"/system/bin/lpm"},
	{feature_safeplace_path,"/system/bin/resize2fs"},
	{feature_safeplace_path,"/system/bin/tune2fs"},
	{feature_safeplace_path,"/system/bin/patchoat"},
	{feature_safeplace_path,"/system/bin/knox_changer"},
	{feature_safeplace_path,"/system/bin/knox_changer_recovery"},
	{feature_safeplace_path,"/sbin/sswap"},
	{feature_safeplace_path,"/sbin/cbd"},
	{feature_safeplace_path,"/sbin/adbd"},
	{feature_safeplace_path,"/sbin/recovery"},
	{feature_safeplace_path,"/sbin/mke2fs_static"},
	{feature_safeplace_path,"/vendor/bin/hw/wpa_supplicant"},
	{feature_safeplace_path,"/vendor/bin/hw/macloader"},
	{feature_safeplace_path,"/vendor/bin/hw/mfgloader"},
	{feature_safeplace_path,"/sbin/dm_verity_hash"},
	{feature_safeplace_path,"/sbin/dm_verity_signature_checker"},
	{feature_safeplace_path,"/vendor/bin/qseecomd"},
	{feature_safeplace_path,"/system/bin/vold_prepare_subdirs"},
	{feature_safeplace_path,"/vendor/bin/init.qcom.early_boot.sh"},
	{feature_safeplace_path,"/vendor/bin/toybox_vendor"},
	{feature_safeplace_path,"/vendor/bin/toolbox"},
	{feature_safeplace_path,"/vendor/bin/hw/android.hardware.usb@1.1-service.wahoo"},
	{feature_safeplace_path,"/vendor/bin/hw/vendor.qti.hardware.iop@2.0-service"},
	{feature_safeplace_path,"/vendor/bin/hw/vendor.qti.hardware.perf@1.0-service"},
	{feature_safeplace_path,"/vendor/bin/init.qcom.class_core.sh"},
	{feature_safeplace_path,"/vendor/bin/irsc_util"},
	{feature_safeplace_path,"/vendor/bin/rmt_storage"},
	{feature_safeplace_path,"/system/bin/toybox"},
	{feature_safeplace_path,"/vendor/bin/init.qcom.usb.sh"},
	{feature_safeplace_path,"/vendor/bin/tftp_server"},
	{feature_safeplace_path,"/vendor/bin/init.qcom.sensors.sh"},
	{feature_safeplace_path,"/system/bin/insthk"},
	{feature_safeplace_path,"/vendor/bin/init.class_main.sh"},
	{feature_safeplace_path,"/vendor/bin/time_daemon"},
	{feature_safeplace_path,"/vendor/bin/thermal-engine"},
	{feature_safeplace_path,"/system/bin/sec_diag_uart_log"},
	{feature_safeplace_path,"/vendor/bin/init.qcom.sh"},
	{feature_safeplace_path,"/system/bin/usbd"},
	{feature_safeplace_path,"/vendor/bin/init.qcom.post_boot.sh"},
	{feature_safeplace_path,"/system/bin/adbd"},
	{feature_safeplace_path,"/system/bin/atrace"},
	{feature_safeplace_path,"/system/bin/fsdbg"},
	{feature_safeplace_path,"/system/bin/dumpsys"},
	{feature_safeplace_path,"/system/bin/logcat"},
	{feature_safeplace_path,"/system/bin/toolbox"},
	{feature_safeplace_path,"/system/bin/mke2fs"},
	{feature_safeplace_path,"/vendor/bin/cbd"},
	{feature_safeplace_path,"/vendor/bin/adsprpcd"},
	{feature_safeplace_path,"/sbin/e2fsdroid_static"},
	{feature_safeplace_path,"/system/bin/e2fsdroid"},
	{feature_safeplace_path,"/system/bin/fsck.f2fs"},
	{feature_safeplace_path,"/system/bin/make_f2fs"},
	{feature_safeplace_path,"/system/bin/sload_f2fs"},
	{feature_safeplace_path,"/system/bin/bpfloader"},
	{feature_safeplace_path,"/system/bin/wait_for_keymaster"},
	{feature_safeplace_path,"/system/bin/secdiscard"},
	{feature_safeplace_path,"/system/bin/idledefrag"},
	{feature_safeplace_path,"/vendor/bin/init.mdm.sh"},
	{feature_safeplace_path,"/vendor/bin/mdm_helper"},
	{feature_safeplace_path,"/vendor/bin/ks"},
	{feature_safeplace_path,"/vendor/bin/sh"},
	{feature_safeplace_path,"/system/bin/e4defrag"},
	{feature_safeplace_path,"/sbin/dm_verity_tz_cmd"},
	{feature_safeplace_path,"/sbin/mcDriverDaemon_static"},
	{feature_safeplace_path,"/sbin/qseecomfsd"},
	{feature_safeplace_path,"/sbin/tzdaemon_recovery"},
	{feature_safeplace_path,"/vendor/bin/hvdcp_opti"},
	{feature_safeplace_path,"/sbin/mkfs.f2fs"},
	{feature_safeplace_path,"/sbin/sload.f2fs"},
	{feature_safeplace_path,"/system/bin/secilc"},
	{feature_safeplace_path,"/system/bin/apexd"},
	{feature_safeplace_path,"/system/bin/art_apex_boot_integrity"},
	{feature_safeplace_path,"/system/bin/gsid"},
	{feature_safeplace_path,"/system/bin/idmap2"},
	{feature_safeplace_path,"/system/bin/charger"},
	{feature_safeplace_path,"/system/bin/recovery"},
	{feature_safeplace_path,"/system/bin/watchdogd"},
	{feature_safeplace_path,"/vendor/bin/hw/vendor.qti.hardware.perf@2.0-service"},
	{feature_safeplace_path,"/system/bin/netutils-wrapper-1.0"},
	{feature_safeplace_path,"/system/bin/bugreport"},
	{feature_safeplace_path,"/system/bin/minadbd"},
	{feature_safeplace_path,"/system/bin/migrate_legacy_obb_data.sh"},
	{feature_safeplace_path,"/vendor/bin/shsusrd"},
	{feature_safeplace_path,"/system/bin/defrag_f2fs"},
	{feature_safeplace_path,"/system/bin/fastbootd"},
	{feature_safeplace_path,"/vendor/bin/hw/vendor.qti.hardware.perf@2.1-service"},
	{feature_safeplace_path,"/vendor/bin/hw/vendor.qti.hardware.perf@2.2-service"},
	{feature_safeplace_path,"/vendor/bin/grep"},
	{feature_safeplace_path,"/vendor/bin/memlogd"},
	{feature_safeplace_path,"/vendor/bin/init.insmod.sh"},
	{feature_safeplace_path,"/vendor/bin/dsmsca"},
	{feature_safeplace_path,"/vendor/bin/hw/android.hardware.usb@1.3-service.coral"},
	{feature_safeplace_path,"/vendor/bin/hw/vendor.qti.hardware.perf-hal-service"},
	{feature_safeplace_path,"/system/bin/rdxd"},
	{feature_safeplace_path,"/system/system_ext/bin/dpmd"},
	{feature_safeplace_path,"/vendor/bin/init.qti.dcvs.sh"},
	{feature_safeplace_path,"/vendor/bin/vendor_modprobe.sh"},
	{feature_safeplace_path,"/vendor/bin/init.qti.qcv.sh"},
	{feature_safeplace_path,"/vendor/bin/init.qcom.crashdata.sh"},
	{feature_safeplace_path,"/vendor/bin/energy-awareness"},
	{feature_safeplace_path,"/vendor/bin/qcom-system-daemon"},
	{feature_safeplace_path,"/vendor/bin/init.qti.kernel.sh"},
	{feature_safeplace_path,"/vendor/bin/init.kernel.post_boot.sh"},
	{feature_safeplace_path,"/vendor/bin/init.kernel.post_boot-lahaina.sh"},
	{feature_safeplace_path,"/vendor/bin/init.qti.keymaster.sh"},
	{feature_safeplace_path,"/vendor/bin/init.qti.early_init.sh"},
	{feature_safeplace_path,"/vendor/bin/thermal_manager"},
	{feature_safeplace_path,"/system/bin/linkerconfig"},
	{feature_safeplace_path,"/system/bin/snapshotctl"},
	{feature_safeplace_path,"/system/bin/boringssl_self_test32"},
	{feature_safeplace_path,"/system/bin/boringssl_self_test64"},
	{feature_safeplace_path,"/vendor/bin/boringssl_self_test32"},
	{feature_safeplace_path,"/vendor/bin/boringssl_self_test64"},
	{feature_safeplace_path,"/apex/com.android.adbd/bin/adbd"},
	{feature_safeplace_path,"/apex/com.android.sdkext/bin/derive_sdk"},
	{feature_safeplace_path,"/apex/com.android.conscrypt/bin/boringssl_self_test32"},
	{feature_safeplace_path,"/apex/com.android.conscrypt/bin/boringssl_self_test64"},
	{feature_safeplace_path,"/system/bin/applypatch"},
	{feature_safeplace_path,"/vendor/bin/applypatch"},
	{feature_safeplace_path,"/system/bin/clean_scratch_files"},
	{feature_safeplace_path,"/system/bin/fsverity"},
	{feature_safeplace_path,"/system/bin/fsverity_init"},
	{feature_safeplace_path,"/system/xbin/librank"},
	{feature_safeplace_path,"/system/xbin/procrank"},
	{feature_safeplace_path,"/system/xbin/showmap"},
	{feature_safeplace_path,"/system/bin/librank"},
	{feature_safeplace_path,"/system/bin/procrank"},
	{feature_safeplace_path,"/system/bin/showmap"},
	{feature_safeplace_path,"/product/bin/dmabuf_dump"},
	{feature_safeplace_path,"/apex/com.android.runtime/bin/spqr"},
	{feature_safeplace_path,"/system/bin/perfetto"},
	{feature_safeplace_path,"/system/bin/update_verifier"},
	{feature_safeplace_path,"/system/bin/bootstrap/linkerconfig"},
	{feature_safeplace_path,"/apex/com.android.runtime/bin/linkerconfig"},
	{feature_safeplace_path,"/system/bin/otapreopt_slot"},
	{feature_safeplace_path,"/apex/com.android.art/bin/dex2oat32"},
	{feature_safeplace_path,"/apex/com.android.art/bin/dex2oat64"},
	{feature_safeplace_path,"/system/bin/incident"},
	{feature_safeplace_path,"/tmp/update_binary;updater_intermediates/updater;obj/EXECUTABLES"},
	{feature_safeplace_path,"/tmp/update-binary"},
	{feature_safeplace_path,"/system/bin/install-recovery.sh"},	/* DEFAULT */
	{feature_safeplace_path,"/vendor/bin/install-recovery.sh"},	/* DEFAULT */
	{feature_safeplace_path,"/system/bin/odsign"},	/* DEFAULT */
	{feature_safeplace_path,"/apex/com.android.art/bin/odrefresh"},	/* DEFAULT */
	{feature_immutable_path_write,"/system/"},	/* DEFAULT */
	{feature_immutable_path_write,"/vendor/"},	/* DEFAULT */
	{feature_immutable_path_open,"/system/bin/"},	/* DEFAULT */
	{feature_immutable_path_open,"/vendor/bin/"},	/* DEFAULT */
	{feature_immutable_src_exception,"/system/bin/icd"},
	{feature_immutable_src_exception,"/system/bin/iof"},
	{feature_immutable_src_exception,"/system/bin/sh"},
	{feature_immutable_src_exception,"/system/bin/app_process32"},
	{feature_immutable_src_exception,"/system/bin/app_process64"},
	{feature_immutable_src_exception,"/system/bin/crash_dump32"},
	{feature_immutable_src_exception,"/system/bin/crash_dump64"},
	{feature_immutable_src_exception,"/system/bin/mediaextractor"},
	{feature_immutable_src_exception,"/system/bin/surfaceflinger"},
	{feature_immutable_src_exception,"/vendor/bin/sh"},
	{feature_immutable_src_exception,"/vendor/bin/hw/android.hardware.media.omx@1.0-service"},
	{feature_immutable_src_exception,"/vendor/bin/snap_utility_32"},
	{feature_immutable_src_exception,"/vendor/bin/snap_utility_64"},
	{feature_immutable_src_exception,"/vendor/bin/icd_vendor"},
	{feature_immutable_src_exception,"/vendor/bin/iof_vendor"},
	{feature_immutable_src_exception,"/init"},
	{feature_immutable_src_exception,"/system/bin/init"},
	{feature_immutable_src_exception,"/system/bin/lshal"},
	/* Rules will be added here */
	/* Never modify the above line. Rules will be added for buildtime */
#endif /* if 0 */
