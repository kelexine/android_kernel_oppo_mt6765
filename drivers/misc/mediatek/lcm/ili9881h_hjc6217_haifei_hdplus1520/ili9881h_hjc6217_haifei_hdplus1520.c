// SPDX-License-Identifier: GPL-2.0
/*
 * ILI9881H INCELL TDDI LCD Panel Driver
 * Panel:      ili9881h_hjc6217_haifei_hdplus1520
 * Resolution: 720 x 1520 (HD+, 20:9)
 * Interface:  MIPI-DSI, 4-lane, SYNC_PULSE_VDO_MODE, RGB888
 * IC:         Ilitek ILI9881H (TDDI — Touch & Display Driver Integration)
 * Module:     HJC6217 / Haifei
 * Platform:   MediaTek MT6762/MT6765
 *
 * Author: kelexine <https://github.com/kelexine>
 */

 #define LOG_TAG "LCM"

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#include "lcm_drv.h"

/* Pinout */
#define LCM_GPIO_RST    503
#define LCM_GPIO_TE     374

/* Panel geometry */
#define LCM_WIDTH       720
#define LCM_HEIGHT      1520

/* DSI clock / timing (verified against stock vmlinux lcm_get_params) */
#define LCM_DSI_PLL_MHZ 270
#define LCM_VSYNC_ACT   4
#define LCM_VBP         8
#define LCM_VFP         246
#define LCM_HSA         8
#define LCM_HBP         38
#define LCM_HFP         40

/* Stock dispatch semantics (verified in stock vmlinux lcm_init loop):
 * 0xAA = skip entry, 0xAB = mdelay(count), 0xFFFB = udelay(count) */
#define REGFLAG_SKIP            0xAA
#define REGFLAG_DELAY           0xAB
#define REGFLAG_UDELAY          0xFFFB

/* MTKFB utility vtable */
static struct LCM_UTIL_FUNCS lcm_util;

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define MDELAY(n)           (lcm_util.mdelay((n)))
#define UDELAY(n)           (lcm_util.udelay((n)))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)        \
    lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)           \
    lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size)                   \
    lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)


struct LCM_setting_table {
	unsigned int  cmd;
	unsigned char count;
	unsigned char para_list[64];
};

/* Init sequence */
static struct LCM_setting_table lcm_initialization_setting[] = {

	/* Page 0 */
	{0xFF, 3, {0x98, 0x81, 0x00}},
	{0x11, 1, {0x00}},
	{REGFLAG_DELAY, 120, {}},

	/* Page 1 */
	{0xFF, 3, {0x98, 0x81, 0x01}},
	{0x00, 1, {0x46}},
	{0x01, 1, {0x16}},
	{0x02, 1, {0x10}},
	{0x03, 1, {0x10}},
	{0x08, 1, {0x80}},
	{0x09, 1, {0x12}},
	{0x0A, 1, {0x71}},
	{0x0B, 1, {0x00}},
	{0x14, 1, {0x8A}},
	{0x15, 1, {0x8A}},
	{0x0C, 1, {0x10}},
	{0x0D, 1, {0x10}},
	{0x0E, 1, {0x00}},
	{0x0F, 1, {0x00}},
	{0x10, 1, {0x01}},
	{0x11, 1, {0x01}},
	{0x12, 1, {0x01}},
	{0x24, 1, {0x00}},
	{0x25, 1, {0x09}},
	{0x26, 1, {0x10}},
	{0x27, 1, {0x10}},
	{0x31, 1, {0x21}},
	{0x32, 1, {0x07}},
	{0x33, 1, {0x01}},
	{0x34, 1, {0x00}},
	{0x35, 1, {0x02}},
	{0x36, 1, {0x07}},
	{0x37, 1, {0x07}},
	{0x38, 1, {0x07}},
	{0x39, 1, {0x07}},
	{0x3A, 1, {0x17}},
	{0x3B, 1, {0x15}},
	{0x3C, 1, {0x07}},
	{0x3D, 1, {0x07}},
	{0x3E, 1, {0x13}},
	{0x3F, 1, {0x11}},
	{0x40, 1, {0x09}},
	{0x41, 1, {0x07}},
	{0x42, 1, {0x07}},
	{0x43, 1, {0x07}},
	{0x44, 1, {0x07}},
	{0x45, 1, {0x07}},
	{0x46, 1, {0x07}},
	{0x47, 1, {0x20}},
	{0x48, 1, {0x07}},
	{0x49, 1, {0x01}},
	{0x4A, 1, {0x00}},
	{0x4B, 1, {0x02}},
	{0x4C, 1, {0x07}},
	{0x4D, 1, {0x07}},
	{0x4E, 1, {0x07}},
	{0x4F, 1, {0x07}},
	{0x50, 1, {0x16}},
	{0x51, 1, {0x14}},
	{0x52, 1, {0x07}},
	{0x53, 1, {0x07}},
	{0x54, 1, {0x12}},
	{0x55, 1, {0x10}},
	{0x56, 1, {0x08}},
	{0x57, 1, {0x07}},
	{0x58, 1, {0x07}},
	{0x59, 1, {0x07}},
	{0x5A, 1, {0x07}},
	{0x5B, 1, {0x07}},
	{0x5C, 1, {0x07}},
	{0x61, 1, {0x08}},
	{0x62, 1, {0x07}},
	{0x63, 1, {0x01}},
	{0x64, 1, {0x00}},
	{0x65, 1, {0x02}},
	{0x66, 1, {0x07}},
	{0x67, 1, {0x07}},
	{0x68, 1, {0x07}},
	{0x69, 1, {0x07}},
	{0x6A, 1, {0x10}},
	{0x6B, 1, {0x12}},
	{0x6C, 1, {0x07}},
	{0x6D, 1, {0x07}},
	{0x6E, 1, {0x14}},
	{0x6F, 1, {0x16}},
	{0x70, 1, {0x20}},
	{0x71, 1, {0x07}},
	{0x72, 1, {0x07}},
	{0x73, 1, {0x07}},
	{0x74, 1, {0x07}},
	{0x75, 1, {0x07}},
	{0x76, 1, {0x07}},
	{0x77, 1, {0x09}},
	{0x78, 1, {0x07}},
	{0x79, 1, {0x01}},
	{0x7A, 1, {0x00}},
	{0x7B, 1, {0x02}},
	{0x7C, 1, {0x07}},
	{0x7D, 1, {0x07}},
	{0x7E, 1, {0x07}},
	{0x7F, 1, {0x07}},
	{0x80, 1, {0x11}},
	{0x81, 1, {0x13}},
	{0x82, 1, {0x07}},
	{0x83, 1, {0x07}},
	{0x84, 1, {0x15}},
	{0x85, 1, {0x17}},
	{0x86, 1, {0x21}},
	{0x87, 1, {0x07}},
	{0x88, 1, {0x07}},
	{0x89, 1, {0x07}},
	{0x8A, 1, {0x07}},
	{0x8B, 1, {0x07}},
	{0x8C, 1, {0x07}},
	{0xA0, 1, {0x01}},
	{0xA1, 1, {0x10}},
	{0xA2, 1, {0x08}},
	{0xA5, 1, {0x10}},
	{0xA6, 1, {0x10}},
	{0xA7, 1, {0x00}},
	{0xA8, 1, {0x00}},
	{0xA9, 1, {0x09}},
	/* stock[121]: 0xAA entry — skipped by the dispatch loop, no DSI write */
	{REGFLAG_SKIP, 1, {0x09}},
	{0xB9, 1, {0x40}},
	{0xd0, 1, {0x01}},
	{0xd1, 1, {0x00}},
	{0xdC, 1, {0x35}},
	{0xdD, 1, {0x42}},
	{0xE2, 1, {0x00}},
	{0xE6, 1, {0x22}},
	{0xE7, 1, {0x54}},

	/* Page 5 */
	{0xFF, 3, {0x98, 0x81, 0x05}},
	{0x58, 1, {0x62}},
	{0x63, 1, {0x88}},
	{0x64, 1, {0x8A}},
	{0x68, 1, {0xAA}},
	{0x69, 1, {0xB1}},
	{0x6A, 1, {0x86}},
	{0x6B, 1, {0x78}},

	/* Page 6 */
	{0xFF, 3, {0x98, 0x81, 0x06}},
	{0x0F, 1, {0x40}},
	{0x11, 1, {0x03}},
	{0x13, 1, {0x54}},
	{0x14, 1, {0x41}},
	{0x15, 1, {0x01}},
	{0x16, 1, {0x41}},
	{0x17, 1, {0xFF}},
	{0x18, 1, {0x00}},
	{0x48, 1, {0x0F}},
	{0x4D, 1, {0x80}},
	{0x4E, 1, {0x40}},

	/* Page 8 */
	{0xFF, 3, {0x98, 0x81, 0x08}},
	{0xE0, 27, {0x40, 0x24, 0x86, 0xC0, 0x05, 0x55, 0x39, 0x60, 0x8D, 0xB1, 0xA9, 0xE6, 0x11, 0x36, 0x5B, 0xEA, 0x83, 0xB7, 0xD9, 0x03, 0xFF, 0x27, 0x54, 0x89, 0xB5, 0x03, 0xFF}},
	{0xE1, 27, {0x40, 0x24, 0x86, 0xC0, 0x05, 0x55, 0x39, 0x60, 0x8D, 0xB1, 0xA9, 0xE6, 0x11, 0x36, 0x5B, 0xEA, 0x83, 0xB7, 0xD9, 0x03, 0xFF, 0x27, 0x54, 0x89, 0xB5, 0x03, 0xFF}},

	/* Page 6 */
	{0xFF, 3, {0x98, 0x81, 0x06}},
	{0xd6, 1, {0x85}},
	{0x27, 1, {0x20}},
	{0x28, 1, {0x20}},
	{0x2E, 1, {0x01}},
	{0xC0, 1, {0xF7}},
	{0xC1, 1, {0x02}},
	{0xC2, 1, {0x04}},

	/* Page 14 */
	{0xFF, 3, {0x98, 0x81, 0x0E}},
	{0x00, 1, {0xA0}},
	{0x01, 1, {0x28}},
	{0x11, 1, {0x90}},
	{0x13, 1, {0x14}},

	/* Page 2 */
	{0xFF, 3, {0x98, 0x81, 0x02}},
	{0x40, 1, {0x43}},
	{0x42, 1, {0x00}},
	{0x4A, 1, {0x08}},
	{0x4D, 1, {0x4E}},
	{0x4E, 1, {0x00}},
	{0x1A, 1, {0x48}},

	/* Page 7 */
	{0xFF, 3, {0x98, 0x81, 0x07}},
	{0x0F, 1, {0x02}},

	/* Page 0 */
	{0xFF, 3, {0x98, 0x81, 0x00}},
	{0x35, 1, {0x00}},
	{0x36, 1, {0x00}},
	{0x29, 1, {0x00}},
	{REGFLAG_DELAY, 20, {}},
	/* stock[180]: 0xAA terminator — skipped by the dispatch loop */
	{REGFLAG_SKIP, 0, {}},
};

/* push_table — inline dispatch replication  */
static void push_table(struct LCM_setting_table *table, unsigned int count,
		       unsigned char force_update)
{
	unsigned int i;

	for (i = 0; i < count; i++) {
		unsigned int cmd = table[i].cmd;

		switch (cmd) {
		case REGFLAG_SKIP:
			break;
		case REGFLAG_DELAY:
			MDELAY(table[i].count);
			break;
		case REGFLAG_UDELAY:
			UDELAY(table[i].count);
			break;
		default:
			dsi_set_cmdq_V2(cmd, table[i].count,
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
	params->width  = LCM_WIDTH;
	params->height = LCM_HEIGHT;
	params->density = 240;

	params->physical_width     = 67;
	params->physical_height    = 142;
	params->physical_width_um  = 67610;
	params->physical_height_um = 142730;

	/* DSI core */
	params->dsi.mode             = BURST_VDO_MODE;
	params->dsi.switch_mode      = CMD_MODE;
	params->dsi.switch_mode_enable = 0;

	params->dsi.LANE_NUM         = LCM_FOUR_LANE;

	params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
	params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
	params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
	params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

	params->dsi.PS                      = LCM_PACKED_PS_24BIT_RGB888;

	/* Vertical timing */
	params->dsi.vertical_sync_active   = LCM_VSYNC_ACT;
	params->dsi.vertical_backporch     = LCM_VBP;
	params->dsi.vertical_frontporch    = LCM_VFP;
	params->dsi.vertical_active_line   = LCM_HEIGHT;

	/* Horizontal timing */
	params->dsi.horizontal_sync_active  = LCM_HSA;
	params->dsi.horizontal_backporch    = LCM_HBP;
	params->dsi.horizontal_frontporch   = LCM_HFP;
	params->dsi.horizontal_active_pixel = LCM_WIDTH;

	/* Clock */
	params->dsi.PLL_CLOCK               = LCM_DSI_PLL_MHZ;
	params->dsi.ssc_disable             = 1;

	/* ESD check */
	params->dsi.esd_check_enable                       = 1;
	params->dsi.customization_esd_check_enable         = 1;
	params->dsi.lcm_esd_check_table[0].cmd            = 0x0A;
	params->dsi.lcm_esd_check_table[0].count          = 1;
	params->dsi.lcm_esd_check_table[0].para_list[0]   = 0x9C;
}

static void lcm_init_power(void)
{
	pr_info("[LCM] ili9881h_hjc6217_haifei_hdplus1520: lcm_init_power (enabling bias)\n");
	SET_RESET_PIN(0);
	MDELAY(30);
	display_bias_enable_uv(5400000);
}

static void lcm_suspend_power(void)
{
	pr_info("[LCM] ili9881h_hjc6217_haifei_hdplus1520 (disabling bias)\n");
	display_bias_disable();
}

static void lcm_resume_power(void)
{
	pr_info("[LCM] ili9881h_hjc6217_haifei_hdplus1520 (enabling bias)\n");
	SET_RESET_PIN(0);
	MDELAY(30);
	display_bias_enable_uv(5400000);
	MDELAY(30);
}

static void lcm_init(void)
{
	pr_info("[LCM] ili9881h_hjc6217_haifei_hdplus1520: lcm_init start\n");
	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);

	push_table(lcm_initialization_setting,
		   ARRAY_SIZE(lcm_initialization_setting), 1);
	pr_info("[LCM] ili9881h_hjc6217_haifei_hdplus1520: lcm_init complete\n");
}

static void lcm_suspend(void)
{
	unsigned int cmd;

	pr_info("[LCM] ili9881h_hjc6217_haifei_hdplus1520: lcm_suspend start\n");
	cmd = 0x00280500;
	dsi_set_cmdq(&cmd, 1, 1);
	MDELAY(10);

	cmd = 0x00100500;
	dsi_set_cmdq(&cmd, 1, 1);
	MDELAY(120);
}

static void lcm_resume(void)
{
	pr_info("[LCM] ili9881h_hjc6217_haifei_hdplus1520: lcm_resume start\n");
	lcm_init();
}

static unsigned int lcm_compare_id(void)
{
	unsigned char buffer[1] = {0};
	unsigned int  array[4];

	SET_RESET_PIN(1);
	MDELAY(10);
	SET_RESET_PIN(0);
	MDELAY(10);
	SET_RESET_PIN(1);
	MDELAY(120);

	/* Switch to ILI9881H page 6 for manufacturer ID register access. */
	array[0] = 0x00043902;   /* DCS long write, 4 bytes */
	array[1] = 0x068198FF;   /* {0xFF, 0x98, 0x81, 0x06} in LE wire order */
	dsi_set_cmdq(array, 2, 1);
	MDELAY(10);

	/* Read manufacturer ID byte from reg 0xF0 — ILI9881H returns 0x98 */
	read_reg_v2(0xF0, buffer, 1);

	return (buffer[0] == 0x98) ? 1 : 0;
}

struct LCM_DRIVER ili9881h_hjc6217_haifei_hdplus1520_lcm_drv = {
	.name           = "ili9881h_hjc6217_haifei_hdplus1520",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params     = lcm_get_params,
	.init           = lcm_init,
	.suspend        = lcm_suspend,
	.resume         = lcm_resume,
	.compare_id     = lcm_compare_id,
	.init_power     = lcm_init_power,
	.resume_power   = lcm_resume_power,
	.suspend_power  = lcm_suspend_power,
};
