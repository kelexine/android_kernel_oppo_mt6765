/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Joey Pan <joey.pan@mediatek.com>
 */

#ifndef __MT65XX_LCM_LIST_H__
#define __MT65XX_LCM_LIST_H__

#include <lcm_drv.h>

/* LCM drivers compiled for the Cubot P50 (see drivers/misc/mediatek/lcm/Makefile) */
extern struct LCM_DRIVER ili9881h_hjc6217_haifei_hdplus1520_lcm_drv;
extern struct LCM_DRIVER icnl9911_boe621_haifei_lhd_lcm_drv;
extern struct LCM_DRIVER nt35695_fhd_dsi_vdo_truly_rt5081_hdp_lcm_drv;
extern struct LCM_DRIVER nt35695B_fhd_dsi_vdo_auo_rt5081_hdp_lcm_drv;

extern struct LCM_DRIVER *lcm_driver_list[];
extern unsigned char lcm_name_list[][128];
extern unsigned int lcm_count;
extern enum LCM_DSI_MODE_CON lcm_dsi_mode;

#ifdef BUILD_LK
extern void mdelay(unsigned long msec);
#endif

#endif
