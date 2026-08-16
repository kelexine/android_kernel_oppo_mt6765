// SPDX-License-Identifier: GPL-2.0
/*
 * LCM Driver for ili9881h_hjc6217_haifei_hdplus1520
 *
 * Author: Onogwu Franklin Kelechi (kelexine) <https://github.com/kelexine>
 * Date: 2026-08-16
 * Purpose: MTK Downstream 4.19.127 LCM Driver
 * Reverse Engineered from stock MTK kernel Image (stock_Image)
 */

#ifdef BUILD_LK
#include <string.h>
#else
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/regulator/consumer.h>
#endif
#include "lcm_drv.h"

#define FRAME_WIDTH                     (720)
#define FRAME_HEIGHT                    (1520)

#define REGFLAG_DELAY                   0xAB
#define REGFLAG_END_OF_TABLE            0xAA

#define LCM_DSI_CMD_MODE                0

static struct LCM_UTIL_FUNCS lcm_util = {0};

#ifndef BUILD_LK
static struct regulator *dsv_pos;
static struct regulator *dsv_neg;
static int is_regulator_inited;
#endif

#define SET_RESET_PIN(v)                (lcm_util.set_reset_pin(v))
#define MDELAY(n)                       (lcm_util.mdelay(n))
#define UDELAY(n)                       (lcm_util.udelay(n))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size) \
        lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
    unsigned int cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table init_setting[] = {
    {0xFF, 3, {0x98, 0x81, 0x00}},
    {0x11, 1, {0x00}},
    {0xAB, 120, {0x00}},
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
    {0xAA, 0, {}},
    {0xB9, 1, {0x40}},
    {0xD0, 1, {0x01}},
    {0xD1, 1, {0x00}},
    {0xDC, 1, {0x35}},
    {0xDD, 1, {0x42}},
    {0xE2, 1, {0x00}},
    {0xE6, 1, {0x22}},
    {0xE7, 1, {0x54}},
    {0xFF, 3, {0x98, 0x81, 0x05}},
    {0x58, 1, {0x62}},
    {0x63, 1, {0x88}},
    {0x64, 1, {0x8A}},
    {0x68, 1, {0xAA}},
    {0x69, 1, {0xB1}},
    {0x6A, 1, {0x86}},
    {0x6B, 1, {0x78}},
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
    {0xFF, 3, {0x98, 0x81, 0x08}},
    {0xE0, 27, {0x40, 0x24, 0x86, 0xC0, 0x05, 0x55, 0x39, 0x60, 0x8D, 0xB1, 0xA9, 0xE6, 0x11, 0x36, 0x5B, 0xEA, 0x83, 0xB7, 0xD9, 0x03, 0xFF, 0x27, 0x54, 0x89, 0xB5, 0x03, 0xFF}},
    {0xE1, 27, {0x40, 0x24, 0x86, 0xC0, 0x05, 0x55, 0x39, 0x60, 0x8D, 0xB1, 0xA9, 0xE6, 0x11, 0x36, 0x5B, 0xEA, 0x83, 0xB7, 0xD9, 0x03, 0xFF, 0x27, 0x54, 0x89, 0xB5, 0x03, 0xFF}},
    {0xFF, 3, {0x98, 0x81, 0x06}},
    {0xD6, 1, {0x85}},
    {0x27, 1, {0x20}},
    {0x28, 1, {0x20}},
    {0x2E, 1, {0x01}},
    {0xC0, 1, {0xF7}},
    {0xC1, 1, {0x02}},
    {0xC2, 1, {0x04}},
    {0xFF, 3, {0x98, 0x81, 0x0E}},
    {0x00, 1, {0xA0}},
    {0x01, 1, {0x28}},
    {0x11, 1, {0x90}},
    {0x13, 1, {0x14}},
    {0xFF, 3, {0x98, 0x81, 0x02}},
    {0x40, 1, {0x43}},
    {0x42, 1, {0x00}},
    {0x4A, 1, {0x08}},
    {0x4D, 1, {0x4E}},
    {0x4E, 1, {0x00}},
    {0x1A, 1, {0x48}},
    {0xFF, 3, {0x98, 0x81, 0x07}},
    {0x0F, 1, {0x02}},
    {0xFF, 3, {0x98, 0x81, 0x00}},
    {0x35, 1, {0x00}},
    {0x36, 1, {0x00}},
    {0x29, 1, {0x00}},
    {0xAB, 20, {0x00}},
    {0xAA, 0, {}},
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;
    for (i = 0; i < count; i++) {
        unsigned int cmd = table[i].cmd;
        switch (cmd) {
        case REGFLAG_DELAY:
            MDELAY(table[i].count);
            break;
        case REGFLAG_END_OF_TABLE:
            break;
        default:
            dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
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

    params->type = LCM_TYPE_DSI;
    params->width = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    params->dsi.mode = BURST_VDO_MODE;
    params->dsi.LANE_NUM = LCM_FOUR_LANE;
    params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq = LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding = LCM_DSI_PADDING_ON_LSB;
    params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;

    params->dsi.intermediat_buffer_num = 2;
    params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;
    params->dsi.word_count = FRAME_WIDTH * 3;

    /* Exact Ground Truth Timings mapped from stock_Image */
    params->dsi.vertical_sync_active = 4;
    params->dsi.vertical_backporch = 8;
    params->dsi.vertical_frontporch = 246;
    params->dsi.vertical_active_line = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active = 8;
    params->dsi.horizontal_backporch = 38;
    params->dsi.horizontal_frontporch = 40;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;

    /* DSI PLL Clock: 270 MHz (0x10E in stock_Image) */
    params->dsi.PLL_CLOCK = 270;
    params->dsi.ssc_disable = 1;

    /* ESD check ground truth */
    params->dsi.customization_esd_check_enable = 1;
    params->dsi.esd_check_enable = 1;
    params->dsi.lcm_esd_check_table[0].cmd = 0x0A;
    params->dsi.lcm_esd_check_table[0].count = 1;
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;

    params->physical_width = 67;
    params->physical_height = 142;
}

static void init_power(void)
{
#ifndef BUILD_LK
    display_bias_enable();
    MDELAY(15);
#else
    SET_RESET_PIN(0);
    MDELAY(30);
#endif
}

static void suspend_power(void)
{
#ifndef BUILD_LK
    SET_RESET_PIN(0);
    MDELAY(2);
    display_bias_disable();
#else
    SET_RESET_PIN(0);
    MDELAY(30);
#endif
}

static void resume_power(void)
{
#ifndef BUILD_LK
    display_bias_enable();
    MDELAY(15);
#endif
}

static void lcm_init(void)
{
    SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(120);

    push_table(init_setting, sizeof(init_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
    unsigned int data_array[16];

    data_array[0] = 0x00280500; /* Display OFF */
    dsi_set_cmdq(data_array, 1, 1);
    MDELAY(10);

    data_array[0] = 0x00100500; /* Sleep IN */
    dsi_set_cmdq(data_array, 1, 1);
    MDELAY(120);
}

static void lcm_resume(void)
{
    lcm_init();
}

static unsigned int lcm_compare_id(void)
{
    unsigned char buffer[4] = {0};
    unsigned int data_array[4];

    SET_RESET_PIN(1);
    MDELAY(10);
    SET_RESET_PIN(0);
    MDELAY(10);
    SET_RESET_PIN(1);
    MDELAY(120);

    /* Switch to Page 1: 0xFF 0x98 0x81 0x01 */
    data_array[0] = 0x00043902;
    data_array[1] = 0x018198FF;
    dsi_set_cmdq(data_array, 2, 1);
    MDELAY(10);

    read_reg_v2(0xF0, buffer, 1);

    return (buffer[0] == 0x98) ? 1 : 0;
}

struct LCM_DRIVER ili9881h_hjc6217_haifei_hdplus1520_lcm_drv = {
    .name                 = "ili9881h_hjc6217_haifei_hdplus1520",
    .set_util_funcs       = lcm_set_util_funcs,
    .get_params           = lcm_get_params,
    .init                 = lcm_init,
    .suspend              = lcm_suspend,
    .resume               = lcm_resume,
    .compare_id           = lcm_compare_id,
    .init_power           = init_power,
    .resume_power         = resume_power,
    .suspend_power        = suspend_power,
    .poweron_before_ulps  = init_power,
    .poweroff_after_ulps  = suspend_power,
};
