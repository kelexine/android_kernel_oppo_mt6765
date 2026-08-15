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
#define LCM_LOGI(fmt, args...)  pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_info("[KERNEL/"LOG_TAG"]"fmt, ##args)
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
	{ REGFLAG_DELAY, 10, {} },
	{ 0x10, 0x00, {} },
	{ REGFLAG_DELAY, 120, {} },
};

/* ILI9881H DCS Initialisation Sequence — verified against stock_Image via Ghidra */
static struct LCM_setting_table init_setting_vdo[] = {
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{ 0x11, 0x01, {0x00} },
	{ REGFLAG_DELAY, 120, {} },

	{ 0xFF, 0x03, {0x98, 0x81, 0x01} },
	{ 0x00, 0x01, {0x46} },
	{ 0x01, 0x01, {0x16} },
	{ 0x02, 0x01, {0x10} },
	{ 0x03, 0x01, {0x10} },
	{ 0x08, 0x01, {0x80} },
	{ 0x09, 0x01, {0x12} },
	{ 0x0A, 0x01, {0x71} },
	{ 0x0B, 0x01, {0x00} },
	{ 0x14, 0x01, {0x8A} },
	{ 0x15, 0x01, {0x8A} },
	{ 0x0C, 0x01, {0x10} },
	{ 0x0D, 0x01, {0x10} },
	{ 0x0E, 0x01, {0x00} },
	{ 0x0F, 0x01, {0x00} },
	{ 0x10, 0x01, {0x01} },
	{ 0x11, 0x01, {0x01} },
	{ 0x12, 0x01, {0x01} },
	{ 0x24, 0x01, {0x00} },
	{ 0x25, 0x01, {0x09} },
	{ 0x26, 0x01, {0x10} },
	{ 0x27, 0x01, {0x10} },
	{ 0x31, 0x01, {0x21} },
	{ 0x32, 0x01, {0x07} },
	{ 0x33, 0x01, {0x01} },
	{ 0x34, 0x01, {0x00} },
	{ 0x35, 0x01, {0x02} },
	{ 0x36, 0x01, {0x07} },
	{ 0x37, 0x01, {0x07} },
	{ 0x38, 0x01, {0x07} },
	{ 0x39, 0x01, {0x07} },
	{ 0x3A, 0x01, {0x17} },
	{ 0x3B, 0x01, {0x15} },
	{ 0x3C, 0x01, {0x07} },
	{ 0x3D, 0x01, {0x07} },
	{ 0x3E, 0x01, {0x13} },
	{ 0x3F, 0x01, {0x11} },
	{ 0x40, 0x01, {0x09} },
	{ 0x41, 0x01, {0x07} },
	{ 0x42, 0x01, {0x07} },
	{ 0x43, 0x01, {0x07} },
	{ 0x44, 0x01, {0x07} },
	{ 0x45, 0x01, {0x07} },
	{ 0x46, 0x01, {0x07} },
	{ 0x47, 0x01, {0x20} },
	{ 0x48, 0x01, {0x07} },
	{ 0x49, 0x01, {0x01} },
	{ 0x4A, 0x01, {0x00} },
	{ 0x4B, 0x01, {0x02} },
	{ 0x4C, 0x01, {0x07} },
	{ 0x4D, 0x01, {0x07} },
	{ 0x4E, 0x01, {0x07} },
	{ 0x4F, 0x01, {0x07} },
	{ 0x50, 0x01, {0x16} },
	{ 0x51, 0x01, {0x14} },
	{ 0x52, 0x01, {0x07} },
	{ 0x53, 0x01, {0x07} },
	{ 0x54, 0x01, {0x12} },
	{ 0x55, 0x01, {0x10} },
	{ 0x56, 0x01, {0x08} },
	{ 0x57, 0x01, {0x07} },
	{ 0x58, 0x01, {0x07} },
	{ 0x59, 0x01, {0x07} },
	{ 0x5A, 0x01, {0x07} },
	{ 0x5B, 0x01, {0x07} },
	{ 0x5C, 0x01, {0x07} },
	{ 0x61, 0x01, {0x08} },
	{ 0x62, 0x01, {0x07} },
	{ 0x63, 0x01, {0x01} },
	{ 0x64, 0x01, {0x00} },
	{ 0x65, 0x01, {0x02} },
	{ 0x66, 0x01, {0x07} },
	{ 0x67, 0x01, {0x07} },
	{ 0x68, 0x01, {0x07} },
	{ 0x69, 0x01, {0x07} },
	{ 0x6A, 0x01, {0x10} },
	{ 0x6B, 0x01, {0x12} },
	{ 0x6C, 0x01, {0x07} },
	{ 0x6D, 0x01, {0x07} },
	{ 0x6E, 0x01, {0x14} },
	{ 0x6F, 0x01, {0x16} },
	{ 0x70, 0x01, {0x20} },
	{ 0x71, 0x01, {0x07} },
	{ 0x72, 0x01, {0x07} },
	{ 0x73, 0x01, {0x07} },
	{ 0x74, 0x01, {0x07} },
	{ 0x75, 0x01, {0x07} },
	{ 0x76, 0x01, {0x07} },
	{ 0x77, 0x01, {0x09} },
	{ 0x78, 0x01, {0x07} },
	{ 0x79, 0x01, {0x01} },
	{ 0x7A, 0x01, {0x00} },
	{ 0x7B, 0x01, {0x02} },
	{ 0x7C, 0x01, {0x07} },
	{ 0x7D, 0x01, {0x07} },
	{ 0x7E, 0x01, {0x07} },
	{ 0x7F, 0x01, {0x07} },
	{ 0x80, 0x01, {0x11} },
	{ 0x81, 0x01, {0x13} },
	{ 0x82, 0x01, {0x07} },
	{ 0x83, 0x01, {0x07} },
	{ 0x84, 0x01, {0x15} },
	{ 0x85, 0x01, {0x17} },
	{ 0x86, 0x01, {0x21} },
	{ 0x87, 0x01, {0x07} },
	{ 0x88, 0x01, {0x07} },
	{ 0x89, 0x01, {0x07} },
	{ 0x8A, 0x01, {0x07} },
	{ 0x8B, 0x01, {0x07} },
	{ 0x8C, 0x01, {0x07} },
	{ 0xA0, 0x01, {0x01} },
	{ 0xA1, 0x01, {0x10} },
	{ 0xA2, 0x01, {0x08} },
	{ 0xA5, 0x01, {0x10} },
	{ 0xA6, 0x01, {0x10} },
	{ 0xA7, 0x01, {0x00} },
	{ 0xA8, 0x01, {0x00} },
	{ 0xA9, 0x01, {0x09} },
	{ 0xB9, 0x01, {0x40} },
	{ 0xD0, 0x01, {0x01} },
	{ 0xD1, 0x01, {0x00} },
	{ 0xDC, 0x01, {0x35} },
	{ 0xDD, 0x01, {0x42} },
	{ 0xE2, 0x01, {0x00} },
	{ 0xE6, 0x01, {0x22} },
	{ 0xE7, 0x01, {0x54} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x05} },
	{ 0x58, 0x01, {0x62} },
	{ 0x63, 0x01, {0x88} },
	{ 0x64, 0x01, {0x8A} },
	{ 0x68, 0x01, {0xAA} },
	{ 0x69, 0x01, {0xB1} },
	{ 0x6A, 0x01, {0x86} },
	{ 0x6B, 0x01, {0x78} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x06} },
	{ 0x0F, 0x01, {0x40} },
	{ 0x11, 0x01, {0x03} },
	{ 0x13, 0x01, {0x54} },
	{ 0x14, 0x01, {0x41} },
	{ 0x15, 0x01, {0x01} },
	{ 0x16, 0x01, {0x41} },
	{ 0x17, 0x01, {0xFF} },
	{ 0x18, 0x01, {0x00} },
	{ 0x48, 0x01, {0x0F} },
	{ 0x4D, 0x01, {0x80} },
	{ 0x4E, 0x01, {0x40} },

	{ 0xFF, 0x03, {0x98, 0x81, 0x08} },
	{ 0xE0, 0x1B, {0x40, 0x24, 0x86, 0xC0, 0x05, 0x55, 0x39, 0x60,
			0x8D, 0xB1, 0xA9, 0xE6, 0x11, 0x36, 0x5B, 0xEA,
			0x83, 0xB7, 0xD9, 0x03, 0xFF, 0x27, 0x54, 0x89,
			0xB5, 0x03, 0xFF} },
	{ 0xE1, 0x1B, {0x40, 0x24, 0x86, 0xC0, 0x05, 0x55, 0x39, 0x60,
			0x8D, 0xB1, 0xA9, 0xE6, 0x11, 0x36, 0x5B, 0xEA,
			0x83, 0xB7, 0xD9, 0x03, 0xFF, 0x27, 0x54, 0x89,
			0xB5, 0x03, 0xFF} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x06} },
	{ 0xD6, 0x01, {0x85} },
	{ 0x27, 0x01, {0x20} },
	{ 0x28, 0x01, {0x20} },
	{ 0x2E, 0x01, {0x01} },
	{ 0xC0, 0x01, {0xF7} },
	{ 0xC1, 0x01, {0x02} },
	{ 0xC2, 0x01, {0x04} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x0E} },
	{ 0x00, 0x01, {0xA0} },
	{ 0x01, 0x01, {0x28} },
	{ 0x11, 0x01, {0x90} },
	{ 0x13, 0x01, {0x14} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x02} },
	{ 0x40, 0x01, {0x43} },
	{ 0x42, 0x01, {0x00} },
	{ 0x4A, 0x01, {0x08} },
	{ 0x4D, 0x01, {0x4E} },
	{ 0x4E, 0x01, {0x00} },
	{ 0x1A, 0x01, {0x48} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x07} },
	{ 0x0F, 0x01, {0x02} },
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },   /* Back to Page 0 */
	{ 0x36, 0x01, {0x00} },                /* MADCTL — scan direction normal */
	{ 0x29, 0x00, {} },                    /* Display On — LAST DCS command */
	{ REGFLAG_DELAY, 20, {} },
	{ 0x35, 0x01, {0x00} },               /* TE on (vsync only) — after display on */
};

static struct LCM_setting_table bl_level[] = {
	{ 0xFF, 0x03, {0x98, 0x81, 0x00} },
	{ 0x51, 0x02, {0x0F, 0xFF} },
	{ REGFLAG_END_OF_TABLE, 0x00, {} },
};

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

static void lcm_set_util_funcs(const struct LCM_UTIL_FUNCS *util)
{
	memcpy(&lcm_util, util, sizeof(struct LCM_UTIL_FUNCS));
}

static void lcm_get_params(struct LCM_PARAMS *params)
{
	memset(params, 0, sizeof(struct LCM_PARAMS));

	params->type   = LCM_TYPE_DSI;
	params->width  = FRAME_WIDTH;
	params->height = FRAME_HEIGHT;

	params->physical_width    = LCM_PHYSICAL_WIDTH  / 1000;
	params->physical_height   = LCM_PHYSICAL_HEIGHT / 1000;
	params->physical_width_um = LCM_PHYSICAL_WIDTH;
	params->physical_height_um = LCM_PHYSICAL_HEIGHT;

	params->dsi.mode        = SYNC_PULSE_VDO_MODE;
	params->dsi.switch_mode = CMD_MODE;
	params->dsi.switch_mode_enable = 0;
	params->dsi.LANE_NUM = LCM_FOUR_LANE;
	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;
	params->dsi.PS                      = LCM_PACKED_PS_24BIT_RGB888;
	params->dsi.packet_size             = 256;

	params->dsi.vertical_sync_active             = 38;
	params->dsi.vertical_backporch               = 40;
	params->dsi.vertical_frontporch              = 270;
	params->dsi.vertical_frontporch_for_low_power = 540;
	params->dsi.vertical_active_line             = FRAME_HEIGHT;
	params->dsi.horizontal_sync_active  = 8;
	params->dsi.horizontal_backporch    = 10;
	params->dsi.horizontal_frontporch   = 36;
	params->dsi.horizontal_active_pixel = FRAME_WIDTH;
	params->dsi.ssc_disable = 1;
	params->dsi.PLL_CLOCK = 246;
	params->dsi.clk_lp_per_line_enable = 0;
	params->dsi.esd_check_enable              = 1;
	params->dsi.customization_esd_check_enable = 0;
	params->dsi.lcm_esd_check_table[0].cmd        = 0x0A;
	params->dsi.lcm_esd_check_table[0].count      = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
}

static void lcm_init_power(void)
{
	/*
	 * Power-on sequence (MT6370 DSV, ILI9881H datasheet §6.2):
	 *   1. Enable AVDD (+5.5V) and AVEE (-5.5V) — MT6370 DSV
	 *   2. Wait ≥30ms for rails to stabilise
	 *   3. Only then drive RST — handled by lcm_init()
	 *
	 * DO NOT assert RST here; bias must be stable first.
	 */
	display_bias_enable();
	MDELAY(30);
}

static void lcm_suspend_power(void)
{
	SET_RESET_PIN(0);
	MDELAY(2);
	display_bias_disable();
}

static void lcm_resume_power(void)
{
	/*
	 * Same constraint as lcm_init_power: bias first, then RST.
	 * lcm_resume() → lcm_init() handles the reset sequence.
	 */
	display_bias_enable();
	MDELAY(30);
}

static void lcm_init(void)
{
	/*
	 * Reset sequence (ILI9881H datasheet §6.2 + lk_lk.bin @ 0x4802bb04):
	 *   RST HIGH → 10ms → RST LOW → 10ms → RST HIGH → 50ms
	 *
	 * After 50ms, lcm_init_power() has already stabilised AVDD/AVEE.
	 * The 120ms Sleep-Out delay is in the DCS table (init_setting_vdo[0]).
	 */
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(50);   /* LK-verified: 50ms post-deassert, NOT 120ms */

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

}

static unsigned int lcm_compare_id(void)
{
	unsigned char buffer[2] = {0};
	unsigned int array[4];

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);

	/* Switch to Page 6 (DCS packet: 0xFF 0x98 0x81 0x06) */
	array[0] = 0x00043902;
	array[1] = 0x068198FF;
	dsi_set_cmdq(array, 2, 1);
	MDELAY(10);

	read_reg_v2(0xF0, buffer, 1);
	LCM_LOGI("%s: id=0x%02x (expected 0x%02x)\n", __func__, buffer[0], LCM_ID_BYTE0);

	return (buffer[0] == LCM_ID_BYTE0) ? 1 : 0;
}

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
