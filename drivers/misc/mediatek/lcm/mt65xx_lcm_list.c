// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#include "mt65xx_lcm_list.h"
#include <lcm_drv.h>
#ifdef BUILD_LK
#include <platform/disp_drv_platform.h>
#else
#include <linux/delay.h>
#endif

enum LCM_DSI_MODE_CON lcm_dsi_mode;

/*
 * LCM driver list for the Cubot P50. A driver is listed here when its
 * directory is added to LCM_LISTS in drivers/misc/mediatek/lcm/Makefile
 * (CONFIG_CUSTOM_KERNEL_LCM plus the two Haifei panels).
 */
struct LCM_DRIVER *lcm_driver_list[] = {
#if defined(ILI9881H_HJC6217_HAIFEI_HDPLUS1520)
	&ili9881h_hjc6217_haifei_hdplus1520_lcm_drv,
#endif

#if defined(ICNL9911_BOE621_HAIFEI_LHD)
	&icnl9911_boe621_haifei_lhd_lcm_drv,
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_HDP)
	&nt35695_fhd_dsi_vdo_truly_rt5081_hdp_lcm_drv,
#endif

#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_HDP)
	&nt35695B_fhd_dsi_vdo_auo_rt5081_hdp_lcm_drv,
#endif
};

unsigned char lcm_name_list[][128] = {
#if defined(ILI9881H_HJC6217_HAIFEI_HDPLUS1520)
	"ili9881h_hjc6217_haifei_hdplus1520_drv",
#endif

#if defined(ICNL9911_BOE621_HAIFEI_LHD)
	"icnl9911_boe621_haifei_lhd_drv",
#endif

#if defined(NT35695_FHD_DSI_VDO_TRULY_RT5081_HDP)
	"nt35695_fhd_dsi_vdo_truly_rt5081_hdp_drv",
#endif

#if defined(NT35695B_FHD_DSI_VDO_AUO_RT5081_HDP)
	"nt35695B_fhd_dsi_vdo_auo_rt5081_hdp_drv",
#endif
};

#define LCM_COMPILE_ASSERT(condition) \
	LCM_COMPILE_ASSERT_X(condition, __LINE__)
#define LCM_COMPILE_ASSERT_X(condition, line) \
	LCM_COMPILE_ASSERT_XX(condition, line)
#define LCM_COMPILE_ASSERT_XX(condition, line) \
	char assertion_failed_at_line_##line[(condition) ? 1 : -1]

unsigned int lcm_count =
	sizeof(lcm_driver_list) / sizeof(struct LCM_DRIVER *);
LCM_COMPILE_ASSERT(sizeof(lcm_driver_list) / sizeof(struct LCM_DRIVER *) != 0);
