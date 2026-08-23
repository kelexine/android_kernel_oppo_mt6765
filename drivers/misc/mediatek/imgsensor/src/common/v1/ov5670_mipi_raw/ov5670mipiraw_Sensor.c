// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "ov5670mipiraw_Sensor.h"

#define PFX "OV5670_camera_sensor"
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static struct imgsensor_info_struct imgsensor_info = {
	.sensor_id = OV5670MIPI_SENSOR_ID,
	.checksum_value = 0x56705670,
	.pre = {
		.pclk = 96000000,
		.linelength  = 1676,
		.framelength = 2045,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.cap = {
		.pclk = 96000000,
		.linelength  = 1676,
		.framelength = 2045,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk = 96000000,
		.linelength  = 1676,
		.framelength = 2045,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 2592,
		.grabwindow_height = 1944,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 150,
	},
	.normal_video = {
		.pclk = 96000000,
		.linelength  = 1676,
		.framelength = 2045,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 96000000,
		.linelength  = 1676,
		.framelength = 1022,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1280,
		.grabwindow_height = 720,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
	},
	.slim_video = {
		.pclk = 96000000,
		.linelength  = 1676,
		.framelength = 2045,
		.startx = 0,
		.starty = 0,
		.grabwindow_width  = 1296,
		.grabwindow_height = 972,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.margin = 4,
	.min_shutter = 4,
	.max_frame_length = 0x7fff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,
	.ihdr_le_firstline = 0,
	.sensor_mode_num = 5,
	.cap_delay_frame = 2,
	.pre_delay_frame = 2,
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_B,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_2_LANE,
	.i2c_addr_table = {0x6c, 0x20, 0x42, 0xff},
};

static struct imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,
	.sensor_mode = IMGSENSOR_MODE_INIT,
	.shutter = 0x3D0,
	.gain = 0x100,
	.dummy_pixel = 0,
	.dummy_line = 0,
	.current_fps = 300,
	.autoflicker_en = KAL_FALSE,
	.test_pattern = KAL_FALSE,
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,
	.ihdr_en = 0,
	.i2c_write_id = 0x6c,
};

static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
	{ 2592, 1944, 0, 0, 2592, 1944, 1296, 972, 0, 0, 1296, 972, 0, 0, 1296, 972 }, /* Preview */
	{ 2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0, 0, 2592, 1944, 0, 0, 2592, 1944 }, /* Capture */
	{ 2592, 1944, 0, 0, 2592, 1944, 1296, 972, 0, 0, 1296, 972, 0, 0, 1296, 972 }, /* Video */
	{ 2592, 1944, 16, 252, 2560, 1440, 1280, 720, 0, 0, 1280, 720, 0, 0, 1280, 720 }, /* HS Video */
	{ 2592, 1944, 0, 0, 2592, 1944, 1296, 972, 0, 0, 1296, 972, 0, 0, 1296, 972 }  /* Slim Video */
};

static kal_uint16 read_cmos_sensor(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint16 addr, kal_uint8 para)
{
	char pu_send_cmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

	iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy(void)
{
	write_cmos_sensor(0x380e, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x380f, imgsensor.frame_length & 0xFF);
	write_cmos_sensor(0x380c, imgsensor.line_length >> 8);
	write_cmos_sensor(0x380d, imgsensor.line_length & 0xFF);
}

static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
	kal_uint32 frame_length = imgsensor.frame_length;

	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ?
				  frame_length : imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
		imgsensor.frame_length = imgsensor_info.max_frame_length;
		imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
	}
	if (min_framelength_en)
		imgsensor.min_frame_length = imgsensor.frame_length;
	spin_unlock(&imgsensor_drv_lock);
	set_dummy();
}

static void write_shutter(kal_uint32 shutter)
{
	kal_uint32 min_framelength = imgsensor.min_frame_length;

	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = min_framelength;

	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;

	spin_lock(&imgsensor_drv_lock);
	set_dummy();
	spin_unlock(&imgsensor_drv_lock);

	shutter = shutter << 4;
	write_cmos_sensor(0x3500, (shutter >> 16) & 0x0F);
	write_cmos_sensor(0x3501, (shutter >> 8) & 0xFF);
	write_cmos_sensor(0x3502, shutter & 0xFF);
}

static void set_shutter(kal_uint32 shutter)
{
	unsigned long flags;

	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	write_shutter(shutter);
}

static kal_uint16 set_gain(kal_uint16 gain)
{
	kal_uint16 reg_gain;

	if (gain < 64)
		gain = 64;
	else if (gain > 1024)
		gain = 1024;

	reg_gain = (gain * 16) / 64;
	write_cmos_sensor(0x3508, (reg_gain >> 8) & 0x07);
	write_cmos_sensor(0x3509, reg_gain & 0xFF);

	return gain;
}

static void streaming_control(kal_bool enable)
{
	if (enable)
		write_cmos_sensor(0x0100, 0x01);
	else
		write_cmos_sensor(0x0100, 0x00);
}

static void sensor_init(void)
{
	write_cmos_sensor(0x0103, 0x01);
	mdelay(5);
	write_cmos_sensor(0x0100, 0x00);

	/* PLL */
	write_cmos_sensor(0x0300, 0x04);
	write_cmos_sensor(0x0301, 0x00);
	write_cmos_sensor(0x0302, 0x78);
	write_cmos_sensor(0x0303, 0x00);
	write_cmos_sensor(0x0304, 0x03);
	write_cmos_sensor(0x0305, 0x01);
	write_cmos_sensor(0x0306, 0x01);
	write_cmos_sensor(0x030a, 0x00);
	write_cmos_sensor(0x030b, 0x00);
	write_cmos_sensor(0x030c, 0x00);
	write_cmos_sensor(0x030d, 0x1e);
	write_cmos_sensor(0x030e, 0x00);
	write_cmos_sensor(0x030f, 0x06);
	write_cmos_sensor(0x0312, 0x01);

	write_cmos_sensor(0x3000, 0x00);
	write_cmos_sensor(0x3002, 0x21);
	write_cmos_sensor(0x3005, 0xf0);
	write_cmos_sensor(0x3007, 0x00);
	write_cmos_sensor(0x3015, 0x0f);
	write_cmos_sensor(0x3018, 0x32);
	write_cmos_sensor(0x301a, 0xf0);
	write_cmos_sensor(0x301b, 0xf0);
	write_cmos_sensor(0x301c, 0xf0);
	write_cmos_sensor(0x301d, 0xf0);
	write_cmos_sensor(0x301e, 0xf0);
	write_cmos_sensor(0x3030, 0x00);
	write_cmos_sensor(0x3031, 0x0a);
	write_cmos_sensor(0x303c, 0xff);
	write_cmos_sensor(0x303e, 0xff);
	write_cmos_sensor(0x3040, 0xf0);
	write_cmos_sensor(0x3041, 0x00);
	write_cmos_sensor(0x3042, 0xf0);
	write_cmos_sensor(0x3106, 0x11);

	write_cmos_sensor(0x3500, 0x00);
	write_cmos_sensor(0x3501, 0x3d);
	write_cmos_sensor(0x3502, 0x00);
	write_cmos_sensor(0x3503, 0x04);
	write_cmos_sensor(0x3504, 0x03);
	write_cmos_sensor(0x3505, 0x83);
	write_cmos_sensor(0x3508, 0x07);
	write_cmos_sensor(0x3509, 0x80);
	write_cmos_sensor(0x350e, 0x04);
	write_cmos_sensor(0x350f, 0x00);
	write_cmos_sensor(0x3510, 0x00);
	write_cmos_sensor(0x3511, 0x02);
	write_cmos_sensor(0x3512, 0x00);

	write_cmos_sensor(0x3601, 0xc8);
	write_cmos_sensor(0x3610, 0x88);
	write_cmos_sensor(0x3612, 0x48);
	write_cmos_sensor(0x3614, 0x5b);
	write_cmos_sensor(0x3615, 0x96);
	write_cmos_sensor(0x3621, 0xd0);
	write_cmos_sensor(0x3622, 0x00);
	write_cmos_sensor(0x3623, 0x00);
	write_cmos_sensor(0x3633, 0x13);
	write_cmos_sensor(0x3634, 0x13);
	write_cmos_sensor(0x3635, 0x13);
	write_cmos_sensor(0x3636, 0x13);
	write_cmos_sensor(0x3645, 0x13);
	write_cmos_sensor(0x3646, 0x82);
	write_cmos_sensor(0x3650, 0x00);
	write_cmos_sensor(0x3652, 0xff);
	write_cmos_sensor(0x3656, 0xff);
	write_cmos_sensor(0x365a, 0xff);
	write_cmos_sensor(0x365e, 0xff);
	write_cmos_sensor(0x3668, 0x00);
	write_cmos_sensor(0x366a, 0x07);
	write_cmos_sensor(0x366e, 0x08);
	write_cmos_sensor(0x366d, 0x00);
	write_cmos_sensor(0x366f, 0x80);

	write_cmos_sensor(0x3700, 0x28);
	write_cmos_sensor(0x3701, 0x10);
	write_cmos_sensor(0x3702, 0x3a);
	write_cmos_sensor(0x3703, 0x19);
	write_cmos_sensor(0x3705, 0x00);
	write_cmos_sensor(0x3706, 0x66);
	write_cmos_sensor(0x3707, 0x08);
	write_cmos_sensor(0x3708, 0x34);
	write_cmos_sensor(0x3709, 0x40);
	write_cmos_sensor(0x370a, 0x01);
	write_cmos_sensor(0x370b, 0x1b);
	write_cmos_sensor(0x3714, 0x24);
	write_cmos_sensor(0x371a, 0x3e);
	write_cmos_sensor(0x3733, 0x00);
	write_cmos_sensor(0x3734, 0x00);
	write_cmos_sensor(0x373a, 0x05);
	write_cmos_sensor(0x373b, 0x06);
	write_cmos_sensor(0x373c, 0x0a);
	write_cmos_sensor(0x373f, 0xa0);
	write_cmos_sensor(0x3755, 0x00);
	write_cmos_sensor(0x3758, 0x00);
	write_cmos_sensor(0x3766, 0x5f);
	write_cmos_sensor(0x3768, 0x00);
	write_cmos_sensor(0x3769, 0x22);
	write_cmos_sensor(0x3773, 0x08);
	write_cmos_sensor(0x3774, 0x1f);
	write_cmos_sensor(0x3776, 0x06);
	write_cmos_sensor(0x37a0, 0x88);
	write_cmos_sensor(0x37a1, 0x5c);
	write_cmos_sensor(0x37a7, 0x88);
	write_cmos_sensor(0x37a8, 0x70);
	write_cmos_sensor(0x37aa, 0x88);
	write_cmos_sensor(0x37ab, 0x48);
	write_cmos_sensor(0x37b3, 0x66);
	write_cmos_sensor(0x37c2, 0x04);
	write_cmos_sensor(0x37c5, 0x00);
	write_cmos_sensor(0x37c8, 0x00);

	write_cmos_sensor(0x3800, 0x00);
	write_cmos_sensor(0x3801, 0x0c);
	write_cmos_sensor(0x3802, 0x00);
	write_cmos_sensor(0x3803, 0x04);
	write_cmos_sensor(0x3804, 0x0a);
	write_cmos_sensor(0x3805, 0x33);
	write_cmos_sensor(0x3806, 0x07);
	write_cmos_sensor(0x3807, 0xa3);
	write_cmos_sensor(0x3808, 0x05);
	write_cmos_sensor(0x3809, 0x10);
	write_cmos_sensor(0x380a, 0x03);
	write_cmos_sensor(0x380b, 0xc0);
	write_cmos_sensor(0x380c, 0x06);
	write_cmos_sensor(0x380d, 0x8c);
	write_cmos_sensor(0x380e, 0x07);
	write_cmos_sensor(0x380f, 0xfd);
	write_cmos_sensor(0x3811, 0x04);
	write_cmos_sensor(0x3813, 0x02);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3815, 0x01);
	write_cmos_sensor(0x3816, 0x00);
	write_cmos_sensor(0x3817, 0x00);
	write_cmos_sensor(0x3818, 0x00);
	write_cmos_sensor(0x3819, 0x00);
	write_cmos_sensor(0x3820, 0x90);
	write_cmos_sensor(0x3821, 0x47);
	write_cmos_sensor(0x3822, 0x48);
	write_cmos_sensor(0x3826, 0x00);
	write_cmos_sensor(0x3827, 0x08);
	write_cmos_sensor(0x382a, 0x03);
	write_cmos_sensor(0x382b, 0x01);
	write_cmos_sensor(0x3830, 0x08);
	write_cmos_sensor(0x3836, 0x02);
	write_cmos_sensor(0x3837, 0x00);
	write_cmos_sensor(0x3838, 0x10);
	write_cmos_sensor(0x3841, 0xff);
	write_cmos_sensor(0x3846, 0x48);
	write_cmos_sensor(0x3861, 0x00);
	write_cmos_sensor(0x3862, 0x00);
	write_cmos_sensor(0x3863, 0x18);
	write_cmos_sensor(0x3a11, 0x01);
	write_cmos_sensor(0x3a12, 0x78);

	write_cmos_sensor(0x4001, 0x60);
	write_cmos_sensor(0x4009, 0x05);
	write_cmos_sensor(0x4043, 0x80);
	write_cmos_sensor(0x4045, 0x80);
	write_cmos_sensor(0x4047, 0x80);
	write_cmos_sensor(0x4049, 0x80);
	write_cmos_sensor(0x4307, 0x30);
	write_cmos_sensor(0x4500, 0x58);
	write_cmos_sensor(0x4501, 0x04);
	write_cmos_sensor(0x4502, 0x48);
	write_cmos_sensor(0x4503, 0x10);
	write_cmos_sensor(0x4508, 0x55);
	write_cmos_sensor(0x4509, 0x55);
	write_cmos_sensor(0x4601, 0x81);
	write_cmos_sensor(0x4700, 0xa4);
	write_cmos_sensor(0x4800, 0x4c);
	write_cmos_sensor(0x4816, 0x53);
	write_cmos_sensor(0x481f, 0x40);
	write_cmos_sensor(0x4830, 0x07);
	write_cmos_sensor(0x4837, 0x11);

	write_cmos_sensor(0x5000, 0x16);
	write_cmos_sensor(0x5001, 0x01);
	write_cmos_sensor(0x5002, 0xa8);
	write_cmos_sensor(0x5004, 0x0c);
	write_cmos_sensor(0x5006, 0x0c);
	write_cmos_sensor(0x5007, 0xe0);
	write_cmos_sensor(0x5008, 0x01);
	write_cmos_sensor(0x5009, 0xb0);
	write_cmos_sensor(0x5a04, 0x0c);
	write_cmos_sensor(0x5a05, 0xe0);
	write_cmos_sensor(0x5a06, 0x09);
	write_cmos_sensor(0x5a07, 0xb0);
	write_cmos_sensor(0x5a08, 0x06);
	write_cmos_sensor(0x3618, 0x2a);
	write_cmos_sensor(0x3734, 0x40);
	write_cmos_sensor(0x4017, 0x10);
	write_cmos_sensor(0x3503, 0x00);
	write_cmos_sensor(0x3d85, 0x17);
	write_cmos_sensor(0x3655, 0x20);
	write_cmos_sensor(0x0100, 0x00);
}

static void preview_setting(void)
{
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3501, 0x3d);
	write_cmos_sensor(0x3623, 0x00);
	write_cmos_sensor(0x366e, 0x08);
	write_cmos_sensor(0x370b, 0x1b);
	write_cmos_sensor(0x3808, 0x05);
	write_cmos_sensor(0x3809, 0x10);
	write_cmos_sensor(0x380a, 0x03);
	write_cmos_sensor(0x380b, 0xcc);
	write_cmos_sensor(0x380c, 0x06);
	write_cmos_sensor(0x380d, 0x8c);
	write_cmos_sensor(0x380e, 0x07);
	write_cmos_sensor(0x380f, 0xfd);
	write_cmos_sensor(0x3814, 0x03);
	write_cmos_sensor(0x3820, 0x90);
	write_cmos_sensor(0x3821, 0x47);
	write_cmos_sensor(0x382a, 0x03);
	write_cmos_sensor(0x4009, 0x05);
	write_cmos_sensor(0x400a, 0x02);
	write_cmos_sensor(0x400b, 0x00);
	write_cmos_sensor(0x4502, 0x48);
	write_cmos_sensor(0x4508, 0x55);
	write_cmos_sensor(0x4509, 0x55);
	write_cmos_sensor(0x450a, 0x00);
	write_cmos_sensor(0x4600, 0x00);
	write_cmos_sensor(0x4601, 0x81);
	write_cmos_sensor(0x4017, 0x10);
	write_cmos_sensor(0x0100, 0x01);
}

static void capture_setting(kal_uint16 currefps)
{
	write_cmos_sensor(0x0100, 0x00);
	write_cmos_sensor(0x3501, 0x7b);
	write_cmos_sensor(0x3623, 0x00);
	write_cmos_sensor(0x366e, 0x10);
	write_cmos_sensor(0x370b, 0x1b);
	write_cmos_sensor(0x3808, 0x0a);
	write_cmos_sensor(0x3809, 0x20);
	write_cmos_sensor(0x380a, 0x07);
	write_cmos_sensor(0x380b, 0x98);
	write_cmos_sensor(0x380c, 0x06);
	write_cmos_sensor(0x380d, 0x8c);
	write_cmos_sensor(0x380e, 0x07);
	write_cmos_sensor(0x380f, 0xfd);
	write_cmos_sensor(0x3814, 0x01);
	write_cmos_sensor(0x3820, 0x80);
	write_cmos_sensor(0x3821, 0x46);
	write_cmos_sensor(0x382a, 0x01);
	write_cmos_sensor(0x4009, 0x0d);
	write_cmos_sensor(0x400a, 0x02);
	write_cmos_sensor(0x400b, 0x00);
	write_cmos_sensor(0x4502, 0x40);
	write_cmos_sensor(0x4508, 0xaa);
	write_cmos_sensor(0x4509, 0xaa);
	write_cmos_sensor(0x450a, 0x00);
	write_cmos_sensor(0x4600, 0x01);
	write_cmos_sensor(0x4601, 0x03);
	write_cmos_sensor(0x4017, 0x08);
	write_cmos_sensor(0x0100, 0x01);
}

static void normal_video_setting(kal_uint16 currefps)
{
	preview_setting();
}

static void hs_video_setting(void)
{
	preview_setting();
}

static void slim_video_setting(void)
{
	preview_setting();
}

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id_reg = 0;

	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		do {
			sensor_id_reg = (read_cmos_sensor(0x300b) << 8) | read_cmos_sensor(0x300c);
			if (sensor_id_reg == imgsensor_info.sensor_id) {
				*sensor_id = imgsensor_info.sensor_id;
				LOG_INF("OV5670 read sensor id: 0x%x\n", *sensor_id);
				return ERROR_NONE;
			}
			LOG_INF("OV5670 Read sensor id fail: 0x%x, retry=%d\n", sensor_id_reg, retry);
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (sensor_id_reg != imgsensor_info.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static kal_uint32 open(void)
{
	kal_uint32 sensor_id = 0;

	if (get_imgsensor_id(&sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	sensor_init();

	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	spin_unlock(&imgsensor_drv_lock);

	return ERROR_NONE;
}

static kal_uint32 close(void)
{
	streaming_control(KAL_FALSE);
	return ERROR_NONE;
}

static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	preview_setting();
	return ERROR_NONE;
}

static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	imgsensor.pclk = imgsensor_info.cap.pclk;
	imgsensor.line_length = imgsensor_info.cap.linelength;
	imgsensor.frame_length = imgsensor_info.cap.framelength;
	imgsensor.min_frame_length = imgsensor_info.cap.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	capture_setting(imgsensor.current_fps);
	return ERROR_NONE;
}

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			       MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
	imgsensor.pclk = imgsensor_info.normal_video.pclk;
	imgsensor.line_length = imgsensor_info.normal_video.linelength;
	imgsensor.frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	normal_video_setting(imgsensor.current_fps);
	return ERROR_NONE;
}

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
	imgsensor.pclk = imgsensor_info.hs_video.pclk;
	imgsensor.line_length = imgsensor_info.hs_video.linelength;
	imgsensor.frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	hs_video_setting();
	return ERROR_NONE;
}

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			     MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);

	slim_video_setting();
	return ERROR_NONE;
}

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;
	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;
	sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;
	sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;

	return ERROR_NONE;
}

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			   MSDK_SENSOR_INFO_STRUCT *sensor_info,
			   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4;
	sensor_info->SensorResetActiveHigh = FALSE;
	sensor_info->SensorResetDelayCount = 5;

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0;
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

	sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
	sensor_info->SensorClockFreq = imgsensor_info.mclk;
	sensor_info->SensorClockDividCount = 3;
	sensor_info->SensorClockRisingCount = 0;
	sensor_info->SensorClockFallingCount = 2;
	sensor_info->SensorPixelClockCount = 3;
	sensor_info->SensorDataLatchCount = 2;

	sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
	sensor_info->SensorWidthSampling = 0;
	sensor_info->SensorHightSampling = 0;
	sensor_info->SensorPacketECCOrder = 1;

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.cap.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;
		break;
	default:
		sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
		sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;
		sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
			imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
		break;
	}

	return ERROR_NONE;
}

static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
			  MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
			  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	spin_lock(&imgsensor_drv_lock);
	imgsensor.current_scenario_id = scenario_id;
	spin_unlock(&imgsensor_drv_lock);

	switch (scenario_id) {
	case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		preview(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
		capture(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
		normal_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
		hs_video(image_window, sensor_config_data);
		break;
	case MSDK_SCENARIO_ID_SLIM_VIDEO:
		slim_video(image_window, sensor_config_data);
		break;
	default:
		preview(image_window, sensor_config_data);
		return ERROR_INVALID_SCENARIO_ID;
	}
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
				  UINT8 *feature_para, UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16 = (UINT16 *)feature_para;
	UINT32 *feature_return_para_32 = (UINT32 *)feature_para;
	unsigned long long *feature_data = (unsigned long long *)feature_para;
	struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;

	switch (feature_id) {
	case SENSOR_FEATURE_GET_PERIOD:
		*feature_return_para_16++ = imgsensor.line_length;
		*feature_return_para_16 = imgsensor.frame_length;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
		*feature_return_para_32 = imgsensor.pclk;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_ESHUTTER:
		set_shutter((kal_uint32)*feature_data);
		break;
	case SENSOR_FEATURE_SET_GAIN:
		set_gain((UINT16)*feature_data);
		break;
	case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
		break;
	case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
		set_max_framerate((UINT16)*feature_data, (kal_bool)*(feature_data + 1));
		break;
	case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
		break;
	case SENSOR_FEATURE_SET_TEST_PATTERN:
		break;
	case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:
		*feature_return_para_32 = imgsensor_info.checksum_value;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_SET_FRAMERATE:
		spin_lock(&imgsensor_drv_lock);
		imgsensor.current_fps = (UINT16)*feature_data;
		spin_unlock(&imgsensor_drv_lock);
		break;
	case SENSOR_FEATURE_SET_HDR:
		break;
	case SENSOR_FEATURE_GET_CROP_INFO:
		wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)*(feature_data + 1);
		switch ((UINT32)*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			memcpy(wininfo, &imgsensor_winsize_info[1], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			memcpy(wininfo, &imgsensor_winsize_info[2], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			memcpy(wininfo, &imgsensor_winsize_info[3], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			memcpy(wininfo, &imgsensor_winsize_info[4], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			memcpy(wininfo, &imgsensor_winsize_info[0], sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
			break;
		}
		break;
	case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		break;
	case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
		*feature_return_para_32 = 0;
		*feature_para_len = 4;
		break;
	case SENSOR_FEATURE_CHECK_SENSOR_ID:
		get_imgsensor_id(feature_return_para_32);
		break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		if (*feature_data != 0)
			set_shutter((kal_uint32)*feature_data);
		streaming_control(KAL_TRUE);
		break;
	default:
		break;
	}
	return ERROR_NONE;
}

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 OV5670_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
	if (pfFunc != NULL)
		*pfFunc = &sensor_func;
	return ERROR_NONE;
}
