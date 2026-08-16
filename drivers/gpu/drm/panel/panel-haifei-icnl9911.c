// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Haifei BOE 6.21" (Chipone ICNL9911) 720x1520 DSI Video Mode Panel
 *
 * Author: Franklin Kelechi (kelexine) <https://github.com/kelexine>
 * Date: 2026-08-16
 * Purpose: Linux DRM Panel driver for Haifei BOE621 ICNL9911 display panel
 * Reverse Engineered from stock MTK vendor kernel (icnl9911_boe621_haifei_lhd)
 */

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct haifei_icnl9911 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *supply_pos;
	struct regulator *supply_neg;
	struct gpio_desc *reset_gpio;
	struct gpio_desc *enable_gpio;
	bool prepared;
};

static inline struct haifei_icnl9911 *to_haifei_icnl9911(struct drm_panel *panel)
{
	return container_of(panel, struct haifei_icnl9911, panel);
}

#define dsi_dcs_write_seq(dsi, seq...)                                         \
	do {                                                                   \
		static const u8 d[] = { seq };                                      \
		int ret;                                                            \
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));            \
		if (ret < 0)                                                        \
			return ret;                                                \
	} while (0)

static int haifei_icnl9911_init_sequence(struct haifei_icnl9911 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;

	/* Chipone Password / Unlock */
	dsi_dcs_write_seq(dsi, 0xf0, 0x5a, 0x59);
	dsi_dcs_write_seq(dsi, 0xf1, 0xa5, 0xa6);

	/* Timing & Driver Registers */
	dsi_dcs_write_seq(dsi, 0xb0, 0x87, 0x86, 0x85, 0x84, 0x02, 0x03, 0x04, 0x05,
			  0x33, 0x33, 0x33, 0x33, 0x00, 0x00, 0x00, 0x78,
			  0x00, 0x00, 0x0f, 0x05, 0x04, 0x03, 0x02, 0x01,
			  0x02, 0x03, 0x04, 0x00, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xb1, 0x53, 0x43, 0x85, 0x80, 0x00, 0x00, 0x00, 0x7e,
			  0x00, 0x00, 0x04, 0x08, 0x54, 0x00, 0x00, 0x00,
			  0x44, 0x40, 0x02, 0x01, 0x40, 0x02, 0x01, 0x40,
			  0x02, 0x01, 0x40, 0x02, 0x01);
	dsi_dcs_write_seq(dsi, 0xb2, 0x54, 0xc4, 0x82, 0x05, 0x40, 0x02, 0x01, 0x40,
			  0x02, 0x01, 0x05, 0x05, 0x54, 0x0c, 0x0c, 0x0d, 0x0b);
	dsi_dcs_write_seq(dsi, 0xb3, 0x02, 0x0c, 0x06, 0x0c, 0x06, 0x26, 0x26, 0x91,
			  0xa2, 0x33, 0x44, 0x00, 0x26, 0x00, 0x18, 0x01,
			  0x02, 0x08, 0x20, 0x30, 0x08, 0x09, 0x44, 0x20,
			  0x40, 0x20, 0x40, 0x08, 0x09, 0x22, 0x33);
	dsi_dcs_write_seq(dsi, 0xb4, 0x00, 0x23, 0x1d, 0x06, 0x04, 0x00, 0x10, 0x12,
			  0x0c, 0x0e, 0x22, 0x1c, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
			  0xfc, 0x60, 0x30, 0x00);
	dsi_dcs_write_seq(dsi, 0xb5, 0x00, 0x23, 0x1d, 0x07, 0x05, 0x00, 0x11, 0x13,
			  0x0d, 0x0f, 0x22, 0x1c, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff,
			  0xfc, 0x60, 0x30, 0x00);
	dsi_dcs_write_seq(dsi, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xbb, 0x01, 0x05, 0x09, 0x11, 0x0d, 0x19, 0x1d, 0x55,
			  0x25, 0x69, 0x00, 0x21, 0x25);
	dsi_dcs_write_seq(dsi, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x02, 0x20, 0xff, 0x00,
			  0x03, 0x33, 0x01, 0x73, 0x33, 0x00);
	dsi_dcs_write_seq(dsi, 0xbd, 0xe9, 0x02, 0x4e, 0xcf, 0x72, 0xa4, 0x08, 0x44,
			  0xae, 0x15);
	dsi_dcs_write_seq(dsi, 0xbe, 0x72, 0x72, 0x46, 0x5a, 0x0c, 0x77, 0x43, 0x07,
			  0x0e, 0x0e);
	dsi_dcs_write_seq(dsi, 0xbf, 0x07, 0x25, 0x07, 0x25, 0x7f, 0x00, 0x11, 0x04);
	dsi_dcs_write_seq(dsi, 0xfa, 0x45, 0x93, 0x01);
	dsi_dcs_write_seq(dsi, 0xf6, 0x3f);
	dsi_dcs_write_seq(dsi, 0xc0, 0x10, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff,
			  0x00);
	dsi_dcs_write_seq(dsi, 0xc1, 0xc0, 0x0c, 0x20, 0x96, 0x04, 0x30, 0x30, 0x04,
			  0x2a, 0xf0, 0x35, 0x00, 0x07, 0xcf, 0xff, 0xff,
			  0x9e, 0x01, 0xc0);
	dsi_dcs_write_seq(dsi, 0xc2, 0x00);
	dsi_dcs_write_seq(dsi, 0xc3, 0x06, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00, 0x81,
			  0x01, 0x00, 0x00);
	dsi_dcs_write_seq(dsi, 0xc4, 0x84, 0x01, 0x2b, 0x41, 0x00, 0x3c, 0x00, 0x03,
			  0x03, 0x2e);
	dsi_dcs_write_seq(dsi, 0xc5, 0x03, 0x1c, 0xb8, 0xb8, 0x30, 0x10, 0x42, 0x44,
			  0x08, 0x09, 0x14);
	dsi_dcs_write_seq(dsi, 0xc6, 0x87, 0x9b, 0x2a, 0x29, 0x29, 0x33, 0x64, 0x34,
			  0x08, 0x04);
	dsi_dcs_write_seq(dsi, 0xc7, 0xf7, 0xdd, 0xc8, 0xb6, 0x93, 0x77, 0x48, 0x99,
			  0x5c, 0x28, 0xf3, 0xb5, 0x03, 0xce, 0xac, 0x7e,
			  0x63, 0x3e, 0x1a, 0x7f, 0xe4, 0x00);
	dsi_dcs_write_seq(dsi, 0xc8, 0xf7, 0xdd, 0xc8, 0xb6, 0x93, 0x77, 0x48, 0x99,
			  0x5c, 0x28, 0xf3, 0xb5, 0x03, 0xce, 0xac, 0x7e,
			  0x63, 0x3e, 0x1a, 0x7f, 0xe4, 0x00);
	dsi_dcs_write_seq(dsi, 0xcb, 0x00);
	dsi_dcs_write_seq(dsi, 0xd0, 0x80, 0x0d, 0xff, 0x0f, 0x61);
	dsi_dcs_write_seq(dsi, 0xd2, 0x42);
	dsi_dcs_write_seq(dsi, 0xfe, 0xff, 0xff, 0xff, 0x40);

	/* Relock registers */
	dsi_dcs_write_seq(dsi, 0xf1, 0x5a, 0x59);
	dsi_dcs_write_seq(dsi, 0xf0, 0xa5, 0xa6);

	/* Turn on TE line & Exit Sleep */
	dsi_dcs_write_seq(dsi, 0x35, 0x00);
	dsi_dcs_write_seq(dsi, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(120);

	/* Turn Display ON */
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_DISPLAY_ON);
	msleep(20);

	return 0;
}

static int haifei_icnl9911_prepare(struct drm_panel *panel)
{
	struct haifei_icnl9911 *ctx = to_haifei_icnl9911(panel);
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_set_voltage(ctx->supply_pos, 5400000, 5400000);
	if (ret)
		dev_warn(panel->dev, "Failed to set supply_pos voltage: %d\n", ret);

	ret = regulator_set_voltage(ctx->supply_neg, 5400000, 5400000);
	if (ret)
		dev_warn(panel->dev, "Failed to set supply_neg voltage: %d\n", ret);

	ret = regulator_enable(ctx->supply_pos);
	if (ret) {
		dev_err(panel->dev, "Failed to enable supply_pos: %d\n", ret);
		return ret;
	}

	udelay(1000);

	ret = regulator_enable(ctx->supply_neg);
	if (ret) {
		dev_err(panel->dev, "Failed to enable supply_neg: %d\n", ret);
		regulator_disable(ctx->supply_pos);
		return ret;
	}

	msleep(20);

	if (ctx->enable_gpio)
		gpiod_set_value_cansleep(ctx->enable_gpio, 1);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(150);

	ret = haifei_icnl9911_init_sequence(ctx);
	if (ret < 0) {
		dev_err(panel->dev, "Failed to send init sequence: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
		if (ctx->enable_gpio)
			gpiod_set_value_cansleep(ctx->enable_gpio, 0);
		regulator_disable(ctx->supply_neg);
		regulator_disable(ctx->supply_pos);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int haifei_icnl9911_unprepare(struct drm_panel *panel)
{
	struct haifei_icnl9911 *ctx = to_haifei_icnl9911(panel);

	if (!ctx->prepared)
		return 0;

	dsi_dcs_write_seq(ctx->dsi, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(10);
	dsi_dcs_write_seq(ctx->dsi, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	if (ctx->enable_gpio)
		gpiod_set_value_cansleep(ctx->enable_gpio, 0);

	regulator_disable(ctx->supply_neg);
	regulator_disable(ctx->supply_pos);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode haifei_icnl9911_default_mode = {
	.clock = 76000,
	.hdisplay = 720,
	.hsync_start = 720 + 261,
	.hsync_end = 720 + 261 + 8,
	.htotal = 720 + 261 + 8 + 1,
	.vdisplay = 1520,
	.vsync_start = 1520 + 48,
	.vsync_end = 1520 + 48 + 4,
	.vtotal = 1520 + 48 + 4 + 48,
	.width_mm = 67,
	.height_mm = 142,
	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int haifei_icnl9911_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &haifei_icnl9911_default_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = haifei_icnl9911_default_mode.width_mm;
	connector->display_info.height_mm = haifei_icnl9911_default_mode.height_mm;
	connector->display_info.bpc = 8;

	return 1;
}

static const struct drm_panel_funcs haifei_icnl9911_panel_funcs = {
	.prepare = haifei_icnl9911_prepare,
	.unprepare = haifei_icnl9911_unprepare,
	.get_modes = haifei_icnl9911_get_modes,
};

static int haifei_icnl9911_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct haifei_icnl9911 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	mipi_dsi_set_drvdata(dsi, ctx);
	ctx->dsi = dsi;

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ctx->supply_pos = devm_regulator_get(dev, "disp-bias-pos");
	if (IS_ERR(ctx->supply_pos)) {
		dev_err(dev, "Failed to get disp-bias-pos regulator\n");
		return PTR_ERR(ctx->supply_pos);
	}

	ctx->supply_neg = devm_regulator_get(dev, "disp-bias-neg");
	if (IS_ERR(ctx->supply_neg)) {
		dev_err(dev, "Failed to get disp-bias-neg regulator\n");
		return PTR_ERR(ctx->supply_neg);
	}

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		dev_err(dev, "Failed to get reset GPIO\n");
		return PTR_ERR(ctx->reset_gpio);
	}

	ctx->enable_gpio = devm_gpiod_get_optional(dev, "enable", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->enable_gpio)) {
		dev_err(dev, "Failed to get enable GPIO\n");
		return PTR_ERR(ctx->enable_gpio);
	}

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &haifei_icnl9911_panel_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		dev_err(dev, "Failed to attach DSI device: %d\n", ret);
		return ret;
	}

	return 0;
}

static int haifei_icnl9911_remove(struct mipi_dsi_device *dsi)
{
	struct haifei_icnl9911 *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id haifei_icnl9911_of_match[] = {
	{ .compatible = "haifei,boe621-icnl9911" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, haifei_icnl9911_of_match);

static struct mipi_dsi_driver haifei_icnl9911_driver = {
	.probe = haifei_icnl9911_probe,
	.remove = haifei_icnl9911_remove,
	.driver = {
		.name = "panel-haifei-icnl9911",
		.of_match_table = haifei_icnl9911_of_match,
	},
};
module_mipi_dsi_driver(haifei_icnl9911_driver);

MODULE_AUTHOR("Franklin Kelechi (kelexine) <https://github.com/kelexine>");
MODULE_DESCRIPTION("DRM Panel Driver for Haifei BOE 6.21 (Chipone ICNL9911) 720x1520");
MODULE_LICENSE("GPL");
