# Copyright (C), 2008-2030, OPPO Mobile Comm Corp., Ltd
### COLOROS_EDIT, All rights reserved.
###
### File: - OplusKernelEnvConfig.mk
### Description:
###     you can get the oplus feature variables set in android side in this file
###     this file will add global macro for common oplus added feature
###     BSP team can do customzation by referring the feature variables
### Version: 1.0
### Date: 2020-03-18
### Author: Liang.Sun
###
### ------------------------------- Revision History: ----------------------------
### <author>                        <date>       <version>   <desc>
### ------------------------------------------------------------------------------
##################################################################################

-include oplus_native_features.mk

###ifdef OPLUS_ARCH_INJECT
OPLUS_CONNECTIVITY_NATIVE_FEATURE_SET :=

##Add OPLUS Debug/Feature Macro Support for kernel/driver
##ifeq ($(OPLUS_FEATURE_TEST), yes)
## OPLUS_CONNECTIVITY_NATIVE_FEATURE_SET += OPLUS_FEATURE_TEST
##endif

ifeq ($(OPLUS_FEATURE_WIFI_MTUDETECT), yes)
OPLUS_CONNECTIVITY_NATIVE_FEATURE_SET += OPLUS_FEATURE_WIFI_MTUDETECT
endif

ifeq ($(OPLUS_FEATURE_WIFI_LIMMITBGSPEED), yes)
OPLUS_CONNECTIVITY_NATIVE_FEATURE_SET += OPLUS_FEATURE_WIFI_LIMMITBGSPEED
endif


$(foreach myfeature,$(OPLUS_CONNECTIVITY_NATIVE_FEATURE_SET),\
    $( \
        $(eval KBUILD_CFLAGS += -D$(myfeature)) \
        $(eval KBUILD_CPPFLAGS += -D$(myfeature)) \
        $(eval CFLAGS_KERNEL += -D$(myfeature)) \
        $(eval CFLAGS_MODULE += -D$(myfeature)) \
    ) \
)
###endif OPLUS_ARCH_INJECT
ALLOWED_MCROS := OPLUS_FEATURE_CAMERA_COMMON \
OPLUS_BUG_COMPATIBILITY \
OPLUS_BUG_STABILITY \
OPLUS_BUG_DEBUG \
OPLUS_ARCH_INJECT \
OPLUS_ARCH_EXTENDS \
VENDOR_EDIT \
COLOROS_EDIT \
OPLUS_FEATURE_POWERINFO_STANDBY \
OPLUS_FEATURE_MTK_ION_SEPARATE_LOCK \
OPLUS_FEATURE_AOD \
OPLUS_FEATURE_DC \
OPLUS_FEATURE_MIPICLKCHANGE \
OPLUS_FEATURE_ZRAM_OPT \
OPLUS_FEATURE_EXFAT_SUPPORT \
OPLUS_FEATURE_SDCARDFS_SUPPORT \
OPLUS_FEATURE_VIRTUAL_RESERVE_MEMORY \
OPLUS_FEATURE_MULTI_FREEAREA \
OPLUS_FEATURE_MULTI_KSWAPD \
OPLUS_FEATURE_PROCESS_RECLAIM \
OPLUS_FEATURE_UIFIRST \
OPLUS_FEATURE_MEMLEAK_DETECT \
OPLUS_FEATURE_STORAGE_TOOL \
OPLUS_FEATURE_MMC_DRIVER \
OPLUS_FEATURE_UFS_DRIVER \
OPLUS_FEATURE_UFS_SHOW_LATENCY \
OPLUS_FEATURE_FG_IO_OPT \
OPLUS_FEATURE_TASK_CPUSTATS \
OPLUS_FEATURE_LOWMEM_DBG



$(foreach myfeature,$(ALLOWED_MCROS),\
         $(eval KBUILD_CFLAGS += -D$(myfeature)) \
         $(eval KBUILD_CPPFLAGS += -D$(myfeature)) \
         $(eval CFLAGS_KERNEL += -D$(myfeature)) \
         $(eval CFLAGS_MODULE += -D$(myfeature)) \
)
