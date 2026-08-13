// SPDX-License-Identifier: GPL-2.0
/*
 * ILI9881H Haifei HJC6217 HD+ 720x1520 LCM Driver
 * Platform : MT6762V/CB (Cubot P50)
 *
 * Timings extracted from lk_lk.bin @ 0x4802ba54 (Ghidra / kelexine)
 * DCS init table: ILI9881H Page 1-8 sequence (RT5081 bias variant)
 * Reset sequence: HIGH(10ms) -> LOW(10ms) -> HIGH(50ms) [LK @ 0x4802bb04]
 *
 * Author: kelexine <https://github.com/kelexine>
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
#include "disp_dts_gpio.h"
#endif

#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif

/* ILI9881H chip ID bytes: 0x98 (reg 0x00), 0x81 (reg 0x01) */
#define LCM_ID_BYTE0        (0x98)
#define LCM_ID_BYTE1        (0x81)

static const unsigned int BL_MIN_LEVEL = 20;
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define MDELAY(n)           (lcm_util.mdelay(n))
#define UDELAY(n)           (lcm_util.udelay(n))

#define dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V22(cmdq, cmd, count, ppara, force_update)
#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)      lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums) \
		lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd) \
	lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size) \
		lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif

/* -----------------------------------------------------------------------
 * Panel geometry
 * Active: 720 x 1520 (HD+ 19:9)
 * Physical: 68 mm x 143 mm (measured from panel spec / DRM mode struct)
 * ----------------------------------------------------------------------- */
#define LCM_DSI_CMD_MODE    0
#define FRAME_WIDTH         (720)
#define FRAME_HEIGHT        (1520)

/* Physical dimensions in micrometers */
#define LCM_PHYSICAL_WIDTH  (68000)
#define LCM_PHYSICAL_HEIGHT (143000)

/* -----------------------------------------------------------------------
 * Pseudo-commands used in the setting table walker
 * ----------------------------------------------------------------------- */
#define REGFLAG_DELAY           0xFFFC
#define REGFLAG_UDELAY          0xFFFB
#define REGFLAG_END_OF_TABLE    0xFFFD
#define REGFLAG_RESET_LOW       0xFFFE
#define REGFLAG_RESET_HIGH      0xFFFF

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

/* -----------------------------------------------------------------------
 * Suspend / Display-off sequence
 * Standard ILI9881H graceful shutdown:
 *   Page 0 -> Display Off (0x28) -> Sleep In (0x10)
 * ----------------------------------------------------------------------- */
static struct LCM_setting_table lcm_suspend_setting[] = {
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{ 0x28, 0x00, {} },
	{ REGFLAG_DELAY, 20, {} },
	{ 0x10, 0x00, {} },
	{ REGFLAG_DELAY, 120, {} },
};

/* -----------------------------------------------------------------------
 * ILI9881H DCS Initialisation Sequence
 *
 * Page unlock prefix:   FF 98 81 <page>
 * Pages used:           01 (GIP / timing), 02 (power), 05 (VCOM/bias),
 *                       06 (misc/esd), 07 (TE), 08 (gamma), 0E (MIPI),
 *                       00 (exit sleep / display on)
 *
 * Source: Oppo MT6765 ili9881h reference driver (same IC, same bias
 * rail RT5081 / MT6370).  Timing registers (VSA/VBP/VFP) live in the
 * DSI controller, not here — they are set via lcm_get_params() below.
 * ----------------------------------------------------------------------- */
static struct LCM_setting_table init_setting_vdo[] = {
	/* ---- Page 1: GIP Forward / Backward scan mapping ---------------- */
	{ 0xFF, 0x03, {0x98, 0x81, 0x01} },
	{ 0x00, 0x01, {0x48} },
	{ 0x01, 0x01, {0x34} },
	{ 0x02, 0x01, {0x35} },
	{ 0x03, 0x01, {0x5E} },
	{ 0x08, 0x01, {0x86} },
	{ 0x09, 0x01, {0x01} },
	{ 0x0a, 0x01, {0x73} },
	{ 0x0b, 0x01, {0x00} },
	{ 0x0c, 0x01, {0x35} },
	{ 0x0d, 0x01, {0x35} },
	{ 0x0e, 0x01, {0x05} },
	{ 0x0f, 0x01, {0x05} },
	{ 0x28, 0x01, {0x48} },
	{ 0x29, 0x01, {0x86} },
	{ 0x2A, 0x01, {0x48} },
	{ 0x2B, 0x01, {0x86} },
	/* Forward scan GIP signal routing */
	{ 0x31, 0x01, {0x07} },
	{ 0x32, 0x01, {0x23} },
	{ 0x33, 0x01, {0x00} },
	{ 0x34, 0x01, {0x0B} },
	{ 0x35, 0x01, {0x09} },
	{ 0x36, 0x01, {0x02} },
	{ 0x37, 0x01, {0x15} },
	{ 0x38, 0x01, {0x17} },
	{ 0x39, 0x01, {0x11} },
	{ 0x3A, 0x01, {0x13} },
	{ 0x3B, 0x01, {0x22} },
	{ 0x3C, 0x01, {0x01} },
	{ 0x3D, 0x01, {0x07} },
	{ 0x3E, 0x01, {0x07} },
	{ 0x3F, 0x01, {0x07} },
	{ 0x40, 0x01, {0x07} },
	{ 0x41, 0x01, {0x07} },
	{ 0x42, 0x01, {0x07} },
	{ 0x43, 0x01, {0x07} },
	{ 0x44, 0x01, {0x07} },
	{ 0x45, 0x01, {0x07} },
	{ 0x46, 0x01, {0x07} },
	{ 0x47, 0x01, {0x07} },
	{ 0x48, 0x01, {0x23} },
	{ 0x49, 0x01, {0x00} },
	{ 0x4A, 0x01, {0x0A} },
	{ 0x4B, 0x01, {0x08} },
	{ 0x4C, 0x01, {0x02} },
	{ 0x4D, 0x01, {0x14} },
	{ 0x4E, 0x01, {0x16} },
	{ 0x4F, 0x01, {0x10} },
	{ 0x50, 0x01, {0x12} },
	{ 0x51, 0x01, {0x22} },
	{ 0x52, 0x01, {0x01} },
	{ 0x53, 0x01, {0x07} },
	{ 0x54, 0x01, {0x07} },
	{ 0x55, 0x01, {0x07} },
	{ 0x56, 0x01, {0x07} },
	{ 0x57, 0x01, {0x07} },
	{ 0x58, 0x01, {0x07} },
	{ 0x59, 0x01, {0x07} },
	{ 0x5a, 0x01, {0x07} },
	{ 0x5b, 0x01, {0x07} },
	{ 0x5c, 0x01, {0x07} },
	/* Backward scan GIP signal routing */
	{ 0x61, 0x01, {0x07} },
	{ 0x62, 0x01, {0x23} },
	{ 0x63, 0x01, {0x00} },
	{ 0x64, 0x01, {0x08} },
	{ 0x65, 0x01, {0x0A} },
	{ 0x66, 0x01, {0x02} },
	{ 0x67, 0x01, {0x12} },
	{ 0x68, 0x01, {0x10} },
	{ 0x69, 0x01, {0x16} },
	{ 0x6a, 0x01, {0x14} },
	{ 0x6b, 0x01, {0x22} },
	{ 0x6c, 0x01, {0x01} },
	{ 0x6d, 0x01, {0x07} },
	{ 0x6e, 0x01, {0x07} },
	{ 0x6f, 0x01, {0x07} },
	{ 0x70, 0x01, {0x07} },
	{ 0x71, 0x01, {0x07} },
	{ 0x72, 0x01, {0x07} },
	{ 0x73, 0x01, {0x07} },
	{ 0x74, 0x01, {0x07} },
	{ 0x75, 0x01, {0x07} },
	{ 0x76, 0x01, {0x07} },
	{ 0x77, 0x01, {0x07} },
	{ 0x78, 0x01, {0x23} },
	{ 0x79, 0x01, {0x00} },
	{ 0x7a, 0x01, {0x09} },
	{ 0x7b, 0x01, {0x0B} },
	{ 0x7c, 0x01, {0x02} },
	{ 0x7d, 0x01, {0x13} },
	{ 0x7e, 0x01, {0x11} },
	{ 0x7f, 0x01, {0x17} },
	{ 0x80, 0x01, {0x15} },
	{ 0x81, 0x01, {0x22} },
	{ 0x82, 0x01, {0x01} },
	{ 0x83, 0x01, {0x07} },
	{ 0x84, 0x01, {0x07} },
	{ 0x85, 0x01, {0x07} },
	{ 0x86, 0x01, {0x07} },
	{ 0x87, 0x01, {0x07} },
	{ 0x88, 0x01, {0x07} },
	{ 0x89, 0x01, {0x07} },
	{ 0x8a, 0x01, {0x07} },
	{ 0x8b, 0x01, {0x07} },
	{ 0x8c, 0x01, {0x07} },
	{ 0xd0, 0x01, {0x01} },
	{ 0xd1, 0x01, {0x00} },
	{ 0xe2, 0x01, {0x00} },
	{ 0xe6, 0x01, {0x22} },
	{ 0xe7, 0x01, {0x54} },
	{ 0xB0, 0x01, {0x33} },
	{ 0xB1, 0x01, {0x33} },
	{ 0xB2, 0x01, {0x00} },
	{ 0xE7, 0x01, {0x54} },

	/* ---- Page 2: Power control --------------------------------------- */
	{ 0xFF, 0x03, {0x98, 0x81, 0x02} },
	{ 0x40, 0x01, {0x52} },
	{ 0x4B, 0x01, {0x5A} },
	{ 0x4D, 0x01, {0x4E} },
	{ 0x1A, 0x01, {0x48} },
	{ 0x4E, 0x01, {0x00} },
	{ 0x1A, 0x01, {0x48} },
	{ 0x70, 0x01, {0x34} },
	{ 0x73, 0x01, {0x0A} },
	{ 0x79, 0x01, {0x06} },

	/* ---- Page 5: VCOM / bias settings -------------------------------- */
	{ 0xFF, 0x03, {0x98, 0x81, 0x05} },
	{ 0x03, 0x01, {0x01} },
	{ 0x04, 0x01, {0x43} },
	{ 0x58, 0x01, {0x63} },
	{ 0x63, 0x01, {0x88} },
	{ 0x64, 0x01, {0x88} },
	{ 0x68, 0x01, {0x65} },
	{ 0x69, 0x01, {0x7F} },
	{ 0x6A, 0x01, {0xC9} },
	{ 0x6B, 0x01, {0xCF} },

	/* ---- Page 6: Misc / ESD / panel control -------------------------- */
	{ 0xFF, 0x03, {0x98, 0x81, 0x06} },
	{ 0x11, 0x01, {0x03} },
	{ 0x13, 0x01, {0x15} },
	{ 0x14, 0x01, {0x41} },
	{ 0x15, 0x01, {0xC2} },
	{ 0x16, 0x01, {0x40} },
	{ 0x17, 0x01, {0x48} },
	{ 0x18, 0x01, {0x3B} },
	{ 0xD6, 0x01, {0x85} },
	{ 0x27, 0x01, {0x20} },
	{ 0x28, 0x01, {0x20} },
	{ 0x2E, 0x01, {0x01} },
	{ 0xC0, 0x01, {0xF7} },
	{ 0xC1, 0x01, {0x02} },
	{ 0xC2, 0x01, {0x04} },
	{ 0x48, 0x01, {0x0F} },
	{ 0x4D, 0x01, {0x80} },
	{ 0x4E, 0x01, {0x40} },
	{ 0x7C, 0x01, {0x40} },
	{ 0x94, 0x01, {0x00} },

	/* ---- Page 7: TE / blanking --------------------------------------- */
	{ 0xFF, 0x03, {0x98, 0x81, 0x07} },
	{ 0x0F, 0x01, {0x02} },

	/* ---- Page 8: Gamma (positive + negative, 39 points each) --------- */
	{ 0xFF, 0x03, {0x98, 0x81, 0x08} },
	{ 0xE0, 0x27, {0x00, 0x24, 0x3C, 0x51, 0x74, 0x40,
			0x97, 0xB6, 0xDE, 0x03, 0x55, 0x42, 0x7A,
			0xAF, 0xE4, 0xAA, 0x1B, 0x5E, 0x88, 0xBB,
			0xFE, 0xE6, 0x1C, 0x5E, 0x93, 0x03, 0xEC} },
	{ 0xE1, 0x27, {0x00, 0x24, 0x3C, 0x51, 0x74, 0x40,
			0x97, 0xB6, 0xDE, 0x03, 0x55, 0x42, 0x7A,
			0xAF, 0xE4, 0xAA, 0x1B, 0x5E, 0x88, 0xBB,
			0xFE, 0xE6, 0x1C, 0x5E, 0x93, 0x03, 0xEC} },

	/* ---- Page 0E: MIPI lane / interface control ---------------------- */
	{ 0xFF, 0x03, {0x98, 0x81, 0x0E} },
	{ 0x00, 0x01, {0xA0} },
	{ 0x13, 0x01, {0x05} },
	{ 0x11, 0x01, {0x90} },

	/* ---- Page 6 second pass: analog tuning after MIPI config --------- */
	{ 0xFF, 0x03, {0x98, 0x81, 0x06} },
	{ 0xD5, 0x01, {0x34} },
	{ 0x58, 0x01, {0xBD} },
	{ 0x94, 0x01, {0x01} },
	{ 0x13, 0x01, {0x4A} },
	{ 0x14, 0x01, {0x2F} },
	{ 0x15, 0x01, {0x0E} },
	{ 0x16, 0x01, {0x2F} },

	/* ---- Page 0: Exit Sleep ------------------------------------------ */
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{ 0x11, 0x00, {} },                     /* Sleep Out (0x11) */
	{ REGFLAG_DELAY, 120, {} },             /* TOFF >= 120 ms per spec  */

	/* ---- Page 2: clear flip register --------------------------------- */
	{ 0xFF, 0x03, {0x98, 0x81, 0x02} },
	{ 0x47, 0x01, {0x00} },

	/* ---- Page 6 final pass ------------------------------------------ */
	{ 0xFF, 0x03, {0x98, 0x81, 0x06} },
	{ 0xD5, 0x01, {0x30} },
	{ 0x58, 0x01, {0xD5} },
	{ 0x94, 0x01, {0x01} },
	{ 0x17, 0x01, {0xFF} },
	{ 0x18, 0x01, {0x00} },

	/* ---- Page 0: Display On ------------------------------------------ */
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{ 0x29, 0x00, {} },                     /* Display On (0x29) */
	{ REGFLAG_DELAY, 20, {} },
	{ 0x35, 0x01, {0x00} },                 /* Tearing Effect Line On */
};

/* Backlight level command (16-bit PWM via 0x51) */
static struct LCM_setting_table bl_level[] = {
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{ 0x51, 0x02, {0x0F, 0xFF} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} },
};

/* -----------------------------------------------------------------------
 * push_table — walk an LCM_setting_table array and dispatch each entry
 * ----------------------------------------------------------------------- */
static void push_table(void *cmdq, struct LCM_setting_table *table,
	unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned int cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;
		switch (cmd) {
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;
		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;
		case REGFLAG_END_OF_TABLE:
			return;
		default:
			dsi_set_cmdq_V22(cmdq, cmd, table[i].count,
				table[i].para_list, force_update);
			break;
		}
	}
}

/* -----------------------------------------------------------------------
 * lcm_set_util_funcs — called by DISP to inject platform callbacks
 * ----------------------------------------------------------------------- */
static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

/* -----------------------------------------------------------------------
 * lcm_get_params — DSI controller configuration
 *
 * Key timings (from lk_lk.bin @ 0x4802ba54):
 *   PLL_CLOCK           = 246 MHz   (4-lane, 60 Hz)
 *   vertical_sync_active= 38  lines  (LK: vsa  = 0x26)
 *   vertical_backporch  = 40  lines  (LK: vbp  = 0x28)
 *   vertical_frontporch = 270 lines  (LK: vfp  = 0x10e)
 *   vtotal              = 1868 lines (1520+38+40+270)
 * ----------------------------------------------------------------------- */
static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	/* Physical panel size in mm (from DRM mode struct: 68 x 143 mm) */
	params->physical_width    = LCM_PHYSICAL_WIDTH  / 1000;
	params->physical_height   = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;

	/* Video mode — SYNC_PULSE required by ILI9881H at 60 Hz on 4-lane */
	params->dsi.mode        = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	params->dsi.switch_mode_enable = 0;

	/* 4-lane MIPI DSI, RGB888 (24-bit) */
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
	params->dsi.PS                      = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.packet_size             = 256;

	/* ------------------------------------------------------------------
	 * Vertical timings — extracted from LK firmware binary
	 * vtotal = 1520 + 270 + 38 + 40 = 1868 lines  (60 Hz @ 246 MHz)
	 * ------------------------------------------------------------------ */
	params->dsi.vertical_sync_active             = 38;
	params->dsi.vertical_backporch               = 40;
	params->dsi.vertical_frontporch              = 270;
	/*
	 * vertical_frontporch_for_low_power: used by DFPS/idle path.
	 * Set to 540 (same ratio as reference) — panel enters LP at ~30 Hz.
	 */
	params->dsi.vertical_frontporch_for_low_power = 540;
	params->dsi.vertical_active_line             = FRAME_HEIGHT;

	/* ------------------------------------------------------------------
	 * Horizontal timings
	 * The LK dump does not expose explicit pixel-domain HFP/HBP/HSA.
	 * Values below are from the closest 60 Hz ILI9881H reference and
	 * are sufficient for initial bring-up.  Tune if you observe tearing.
	 * ------------------------------------------------------------------ */
	params->dsi.horizontal_sync_active  = 8;
	params->dsi.horizontal_backporch    = 10;
	params->dsi.horizontal_frontporch   = 36;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;

	/* SSC disabled — not needed for fixed 60 Hz */
	params->dsi.ssc_disable = 1;

	/* DSI bit-clock: 246 MHz per lane (4-lane, 60 Hz) — from LK 0x4802ba54 */
	params->dsi.PLL_CLOCK = 246;

	/* LP clock gating per-line not needed for video mode */
	params->dsi.clk_lp_per_line_enable = 0;

	/* ------------------------------------------------------------------
	 * ESD detection: read 0x0A, expect 0x9C (display normal / sleep-out)
	 * ------------------------------------------------------------------ */
	params->dsi.esd_check_enable              = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd        = 0x0A;
	params->dsi.lcm_esd_check_table[0].count      = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
}

/* -----------------------------------------------------------------------
 * Power sequencing
 *
 * lcm_init_power  — called once at boot / first init
 * lcm_suspend_power — called before panel-off DCS
 * lcm_resume_power  — called before panel-on DCS
 *
 * display_bias_enable/disable drive VSP (+5.5 V) / VSN (-5.5 V) via the
 * MT6370 sub-PMIC DSV rails (mt6370_dsvp / mt6370_dsvn).
 * The 15 ms delay satisfies the MT6370 ramp-up time before RESET.
 * ----------------------------------------------------------------------- */
static void lcm_init_power(void)
{
	display_bias_enable();
	MDELAY(15);
}

static void lcm_suspend_power(void)
{
	SET_RESET_PIN(0);
	MDELAY(2);
	display_bias_disable();
}

static void lcm_resume_power(void)
{
	display_bias_enable();
	MDELAY(15);
}

/* -----------------------------------------------------------------------
 * lcm_init — hardware reset then DCS init
 *
 * Reset waveform (from lk_lk.bin @ 0x4802bb04 — GPIO 174 / 0xae):
 *   RST HIGH  10 ms
 *   RST LOW   10 ms
 *   RST HIGH  50 ms   (panel stable, ready for DCS)
 * ----------------------------------------------------------------------- */
static void lcm_init(void)
{
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(50);   /* panel ready for commands after 50 ms high */

	push_table(NULL, init_setting_vdo,
		ARRAY_SIZE(init_setting_vdo), 1);

	LCM_LOGI("%s: ili9881h_hjc6217_haifei init done\n", __func__);
}

static void lcm_suspend(void)
{
	push_table(NULL, lcm_suspend_setting,
		ARRAY_SIZE(lcm_suspend_setting), 1);
}

static void lcm_resume(void)
{
	lcm_init();
}

static void lcm_update(unsigned int x, unsigned int y,
	unsigned int width, unsigned int height)
{
	/* Video mode — no partial update */
}

/* -----------------------------------------------------------------------
 * lcm_compare_id
 *
 * ILI9881H identifies itself as:
 *   reg 0x00 = 0x98   (high byte of IC code 0x9881)
 *   reg 0x01 = 0x81   (low byte, also encodes H variant)
 * ----------------------------------------------------------------------- */
static unsigned int lcm_compare_id(void)
{
	unsigned int id = 0, version_id = 0;
	unsigned char buffer[2];
	unsigned int array[16];

	struct LCM_setting_table switch_page1[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x01} }
	};
	struct LCM_setting_table switch_page0[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x00} }
	};

	/* Brief reset to put panel in known state for ID read */
	SET_RESET_PIN(1);
	SET_RESET_PIN(0);
	MDELAY(1);
	SET_RESET_PIN(1);
	MDELAY(20);

	push_table(NULL, switch_page1, ARRAY_SIZE(switch_page1), 1);

	array[0] = 0x00023700;
	dsi_set_cmdq(array, 1, 1);

	read_reg_v2(0x00, buffer, 1);
	id = buffer[0];

	read_reg_v2(0x01, buffer, 1);
	version_id = buffer[0];

	LCM_LOGI("%s: id=0x%02x version=0x%02x\n", __func__, id, version_id);

	push_table(NULL, switch_page0, ARRAY_SIZE(switch_page0), 1);

	return (id == LCM_ID_BYTE0 && version_id == LCM_ID_BYTE1) ? 1 : 0;
}

/* -----------------------------------------------------------------------
 * lcm_esd_check — return TRUE if panel needs recovery
 * ----------------------------------------------------------------------- */
static unsigned int lcm_esd_check(void)
{
	char buffer[3];
	int array[4];

	array[0] = 0x00013700;
	dsi_set_cmdq(array, 1, 1);
	read_reg_v2(0x0A, buffer, 1);

	if (buffer[0] != 0x9C) {
		LCM_LOGI("[LCM ESD] 0x0A=0x%02x (expected 0x9C)\n", buffer[0]);
		return TRUE;
	}
	return FALSE;
}

/* -----------------------------------------------------------------------
 * lcm_ata_check — automated test / factory boundary check
 * ----------------------------------------------------------------------- */
static unsigned int lcm_ata_check(unsigned char *buffer)
{
	unsigned int ret = 0;
	unsigned int x0 = FRAME_WIDTH / 4;
	unsigned char x0_MSB = (x0 & 0xFF);
	unsigned int data_array[2];
	unsigned char read_buf[2];

	struct LCM_setting_table page2[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x02} }
	};
	struct LCM_setting_table page0[] = {
		{ 0xFF, 0x03, {0x98, 0x81, 0x00} }
	};

	MDELAY(20);
	push_table(NULL, page2, ARRAY_SIZE(page2), 1);

	data_array[0] = 0x00023902;
	data_array[1] = (unsigned int)(x0_MSB << 8) | 0x30;
	dsi_set_cmdq(data_array, 2, 1);

	data_array[0] = 0x00013700;
	dsi_set_cmdq(data_array, 1, 1);
	read_reg_v2(0x30, read_buf, 1);

	push_table(NULL, page0, ARRAY_SIZE(page0), 1);

	ret = (read_buf[0] == x0_MSB) ? 1 : 0;
	LCM_LOGI("%s: x0_MSB=0x%x read=0x%x ret=%d\n",
		__func__, x0_MSB, read_buf[0], ret);
	return ret;
}

/* -----------------------------------------------------------------------
 * lcm_setbacklight_cmdq — set panel brightness (16-bit range)
 * ----------------------------------------------------------------------- */
static void lcm_setbacklight_cmdq(void *handle, unsigned int level)
{
	LCM_LOGI("%s: level=%d\n", __func__, level);

	bl_level[1].para_list[0] = (level & 0xFF0) >> 4;
	bl_level[1].para_list[1] = (level & 0x00F) << 4;

	push_table(handle, bl_level, ARRAY_SIZE(bl_level), 1);
}

static void *lcm_switch_mode(int mode)
{
	return NULL;
}

/* -----------------------------------------------------------------------
 * LCM_DRIVER — the name field MUST match the string in the DT /
 * bootloader exactly so disp_lcm_probe() can find us.
 * ----------------------------------------------------------------------- */
struct LCM_DRIVER ili9881h_hjc6217_haifei_hdplus1520_lcm_drv = {
	.name               = "ili9881h_hjc6217_haifei_hdplus1520",
	.set_util_funcs     = lcm_set_util_funcs,
	.get_params         = lcm_get_params,
	.init               = lcm_init,
	.suspend            = lcm_suspend,
	.resume             = lcm_resume,
	.compare_id         = lcm_compare_id,
	.init_power         = lcm_init_power,
	.resume_power       = lcm_resume_power,
	.suspend_power      = lcm_suspend_power,
	.esd_check          = lcm_esd_check,
	.set_backlight_cmdq = lcm_setbacklight_cmdq,
	.ata_check          = lcm_ata_check,
	.update             = lcm_update,
	.switch_mode        = lcm_switch_mode,
};
