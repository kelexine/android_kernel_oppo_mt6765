// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2026 kelexine <https://github.com/kelexine>
 * Weak stubs for stripped OPPO vendor framework functions & variables.
 */

#include <linux/compiler.h>
#include <linux/types.h>

__weak unsigned int is_project(int project) { return 0; }
__weak unsigned int get_project(void) { return 0; }
__weak int get_Operator_Version(void) { return 0; }
__weak int get_eng_version(void) { return 0; }
__weak bool oplus_is_vooc_project(void) { return false; }
__weak int oplus_gauge_init(void) { return 0; }
__weak int oplus_chg_get_ui_soc(void) { return 0; }
__weak int oplus_chg_get_notify_flag(void) { return 0; }
__weak int oplus_get_prop_status(void) { return 0; }
__weak bool oplus_chg_show_vooc_logo_ornot(void) { return false; }
__weak bool oplus_chg_check_chip_is_null(void) { return true; }
__weak void oplus_chg_set_otg_online(int enable) {}
__weak int mtk_chg_enable_vbus_ovp(bool enable) { return 0; }
__weak int chr_get_debug_level(void) { return 0; }
__weak void _wake_up_charger(void) {}
__weak int mt6357_get_vbus_status(void) { return 0; }
__weak void black_screen_timer_restart(void) {}
__weak void bright_screen_timer_restart(void) {}

__weak bool is_reclaim_should_cancel(void) { return false; }
__weak void switch_headset_state(int state) {}
__weak void hans_report(void *arg, ...) {}
__weak u32 OV5670_MIPI_RAW_SensorInit(void *pfFunc) { return 0; }
__weak u32 OV5670_MIPI_RAW_SensorInit_2(void *pfFunc) { return 0; }
__weak int oplus_tchg_01c_precision = 0;
__weak void oppo_init_sensor_state(void) {}
__weak bool oppo_daily_build(void) { return false; }
__weak void print_utc_time(void) {}
__weak int sysctl_slide_boost_enabled = 0;
