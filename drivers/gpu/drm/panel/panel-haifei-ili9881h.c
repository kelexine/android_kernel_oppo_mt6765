// SPDX-License-Identifier: GPL-2.0+
/*
 * Driver for Haifei HJC6217 (Ilitek ILI9881H) 720x1520 DSI Video Mode Panel
 *
 * Author: Franklin Kelechi (kelexine) <https://github.com/kelexine>
 * Date: 2026-08-16
 * Purpose: Linux DRM Panel driver for Haifei HJC6217 ILI9881H display panel
 * Reverse Engineered from stock MTK vendor kernel (ili9881h_hjc6217_haifei_hdplus1520)
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

struct haifei_ili9881h {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator *supply_pos;
	struct regulator *supply_neg;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline struct haifei_ili9881h *to_haifei_ili9881h(struct drm_panel *panel)
{
	return container_of(panel, struct haifei_ili9881h, panel);
}

#define dsi_dcs_write_seq(dsi, seq...)                                         \
	do {                                                                   \
		static const u8 d[] = { seq };                                      \
		int ret;                                                            \
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));            \
		if (ret < 0)                                                        \
			return ret;                                                \
	} while (0)

static int haifei_ili9881h_init_sequence(struct haifei_ili9881h *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;

	/* Switch Page 0 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);
	dsi_dcs_write_seq(dsi, MIPI_DCS_EXIT_SLEEP_MODE);
	msleep(120);

	/* Switch Page 1 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x01);
	dsi_dcs_write_seq(dsi, 0x00, 0x46);
	dsi_dcs_write_seq(dsi, 0x01, 0x16);
	dsi_dcs_write_seq(dsi, 0x02, 0x10);
	dsi_dcs_write_seq(dsi, 0x03, 0x10);
	dsi_dcs_write_seq(dsi, 0x08, 0x80);
	dsi_dcs_write_seq(dsi, 0x09, 0x12);
	dsi_dcs_write_seq(dsi, 0x0a, 0x71);
	dsi_dcs_write_seq(dsi, 0x0b, 0x00);
	dsi_dcs_write_seq(dsi, 0x14, 0x8a);
	dsi_dcs_write_seq(dsi, 0x15, 0x8a);
	dsi_dcs_write_seq(dsi, 0x0c, 0x10);
	dsi_dcs_write_seq(dsi, 0x0d, 0x10);
	dsi_dcs_write_seq(dsi, 0x0e, 0x00);
	dsi_dcs_write_seq(dsi, 0x0f, 0x00);
	dsi_dcs_write_seq(dsi, MIPI_DCS_ENTER_SLEEP_MODE);
	dsi_dcs_write_seq(dsi, MIPI_DCS_EXIT_SLEEP_MODE);
	dsi_dcs_write_seq(dsi, 0x12, 0x01);
	dsi_dcs_write_seq(dsi, 0x24, 0x00);
	dsi_dcs_write_seq(dsi, 0x25, 0x09);
	dsi_dcs_write_seq(dsi, 0x26, 0x10);
	dsi_dcs_write_seq(dsi, 0x27, 0x10);
	dsi_dcs_write_seq(dsi, 0x31, 0x21);
	dsi_dcs_write_seq(dsi, 0x32, 0x07);
	dsi_dcs_write_seq(dsi, 0x33, 0x01);
	dsi_dcs_write_seq(dsi, 0x34, 0x00);
	dsi_dcs_write_seq(dsi, 0x35, 0x02);
	dsi_dcs_write_seq(dsi, 0x36, 0x07);
	dsi_dcs_write_seq(dsi, 0x37, 0x07);
	dsi_dcs_write_seq(dsi, 0x38, 0x07);
	dsi_dcs_write_seq(dsi, 0x39, 0x07);
	dsi_dcs_write_seq(dsi, 0x3a, 0x17);
	dsi_dcs_write_seq(dsi, 0x3b, 0x15);
	dsi_dcs_write_seq(dsi, 0x3c, 0x07);
	dsi_dcs_write_seq(dsi, 0x3d, 0x07);
	dsi_dcs_write_seq(dsi, 0x3e, 0x13);
	dsi_dcs_write_seq(dsi, 0x3f, 0x11);
	dsi_dcs_write_seq(dsi, 0x40, 0x09);
	dsi_dcs_write_seq(dsi, 0x41, 0x07);
	dsi_dcs_write_seq(dsi, 0x42, 0x07);
	dsi_dcs_write_seq(dsi, 0x43, 0x07);
	dsi_dcs_write_seq(dsi, 0x44, 0x07);
	dsi_dcs_write_seq(dsi, 0x45, 0x07);
	dsi_dcs_write_seq(dsi, 0x46, 0x07);
	dsi_dcs_write_seq(dsi, 0x47, 0x20);
	dsi_dcs_write_seq(dsi, 0x48, 0x07);
	dsi_dcs_write_seq(dsi, 0x49, 0x01);
	dsi_dcs_write_seq(dsi, 0x4a, 0x00);
	dsi_dcs_write_seq(dsi, 0x4b, 0x02);
	dsi_dcs_write_seq(dsi, 0x4c, 0x07);
	dsi_dcs_write_seq(dsi, 0x4d, 0x07);
	dsi_dcs_write_seq(dsi, 0x4e, 0x07);
	dsi_dcs_write_seq(dsi, 0x4f, 0x07);
	dsi_dcs_write_seq(dsi, 0x50, 0x16);
	dsi_dcs_write_seq(dsi, 0x51, 0x14);
	dsi_dcs_write_seq(dsi, 0x52, 0x07);
	dsi_dcs_write_seq(dsi, 0x53, 0x07);
	dsi_dcs_write_seq(dsi, 0x54, 0x12);
	dsi_dcs_write_seq(dsi, 0x55, 0x10);
	dsi_dcs_write_seq(dsi, 0x56, 0x08);
	dsi_dcs_write_seq(dsi, 0x57, 0x07);
	dsi_dcs_write_seq(dsi, 0x58, 0x07);
	dsi_dcs_write_seq(dsi, 0x59, 0x07);
	dsi_dcs_write_seq(dsi, 0x5a, 0x07);
	dsi_dcs_write_seq(dsi, 0x5b, 0x07);
	dsi_dcs_write_seq(dsi, 0x5c, 0x07);
	dsi_dcs_write_seq(dsi, 0x61, 0x08);
	dsi_dcs_write_seq(dsi, 0x62, 0x07);
	dsi_dcs_write_seq(dsi, 0x63, 0x01);
	dsi_dcs_write_seq(dsi, 0x64, 0x00);
	dsi_dcs_write_seq(dsi, 0x65, 0x02);
	dsi_dcs_write_seq(dsi, 0x66, 0x07);
	dsi_dcs_write_seq(dsi, 0x67, 0x07);
	dsi_dcs_write_seq(dsi, 0x68, 0x07);
	dsi_dcs_write_seq(dsi, 0x69, 0x07);
	dsi_dcs_write_seq(dsi, 0x6a, 0x10);
	dsi_dcs_write_seq(dsi, 0x6b, 0x12);
	dsi_dcs_write_seq(dsi, 0x6c, 0x07);
	dsi_dcs_write_seq(dsi, 0x6d, 0x07);
	dsi_dcs_write_seq(dsi, 0x6e, 0x14);
	dsi_dcs_write_seq(dsi, 0x6f, 0x16);
	dsi_dcs_write_seq(dsi, 0x70, 0x20);
	dsi_dcs_write_seq(dsi, 0x71, 0x07);
	dsi_dcs_write_seq(dsi, 0x72, 0x07);
	dsi_dcs_write_seq(dsi, 0x73, 0x07);
	dsi_dcs_write_seq(dsi, 0x74, 0x07);
	dsi_dcs_write_seq(dsi, 0x75, 0x07);
	dsi_dcs_write_seq(dsi, 0x76, 0x07);
	dsi_dcs_write_seq(dsi, 0x77, 0x09);
	dsi_dcs_write_seq(dsi, 0x78, 0x07);
	dsi_dcs_write_seq(dsi, 0x79, 0x01);
	dsi_dcs_write_seq(dsi, 0x7a, 0x00);
	dsi_dcs_write_seq(dsi, 0x7b, 0x02);
	dsi_dcs_write_seq(dsi, 0x7c, 0x07);
	dsi_dcs_write_seq(dsi, 0x7d, 0x07);
	dsi_dcs_write_seq(dsi, 0x7e, 0x07);
	dsi_dcs_write_seq(dsi, 0x7f, 0x07);
	dsi_dcs_write_seq(dsi, 0x80, 0x11);
	dsi_dcs_write_seq(dsi, 0x81, 0x13);
	dsi_dcs_write_seq(dsi, 0x82, 0x07);
	dsi_dcs_write_seq(dsi, 0x83, 0x07);
	dsi_dcs_write_seq(dsi, 0x84, 0x15);
	dsi_dcs_write_seq(dsi, 0x85, 0x17);
	dsi_dcs_write_seq(dsi, 0x86, 0x21);
	dsi_dcs_write_seq(dsi, 0x87, 0x07);
	dsi_dcs_write_seq(dsi, 0x88, 0x07);
	dsi_dcs_write_seq(dsi, 0x89, 0x07);
	dsi_dcs_write_seq(dsi, 0x8a, 0x07);
	dsi_dcs_write_seq(dsi, 0x8b, 0x07);
	dsi_dcs_write_seq(dsi, 0x8c, 0x07);
	dsi_dcs_write_seq(dsi, 0xa0, 0x01);
	dsi_dcs_write_seq(dsi, 0xa1, 0x10);
	dsi_dcs_write_seq(dsi, 0xa2, 0x08);
	dsi_dcs_write_seq(dsi, 0xa5, 0x10);
	dsi_dcs_write_seq(dsi, 0xa6, 0x10);
	dsi_dcs_write_seq(dsi, 0xa7, 0x00);
	dsi_dcs_write_seq(dsi, 0xa8, 0x00);
	dsi_dcs_write_seq(dsi, 0xa9, 0x09);
	dsi_dcs_write_seq(dsi, 0xb9, 0x40);
	dsi_dcs_write_seq(dsi, 0xd0, 0x01);
	dsi_dcs_write_seq(dsi, 0xd1, 0x00);
	dsi_dcs_write_seq(dsi, 0xdc, 0x35);
	dsi_dcs_write_seq(dsi, 0xdd, 0x42);
	dsi_dcs_write_seq(dsi, 0xe2, 0x00);
	dsi_dcs_write_seq(dsi, 0xe6, 0x22);
	dsi_dcs_write_seq(dsi, 0xe7, 0x54);

	/* Switch Page 5 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x05);
	dsi_dcs_write_seq(dsi, 0x58, 0x62);
	dsi_dcs_write_seq(dsi, 0x63, 0x88);
	dsi_dcs_write_seq(dsi, 0x64, 0x8a);
	dsi_dcs_write_seq(dsi, 0x68, 0xaa);
	dsi_dcs_write_seq(dsi, 0x69, 0xb1);
	dsi_dcs_write_seq(dsi, 0x6a, 0x86);
	dsi_dcs_write_seq(dsi, 0x6b, 0x78);

	/* Switch Page 6 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x06);
	dsi_dcs_write_seq(dsi, 0x0f, 0x40);
	dsi_dcs_write_seq(dsi, MIPI_DCS_EXIT_SLEEP_MODE);
	dsi_dcs_write_seq(dsi, 0x13, 0x54);
	dsi_dcs_write_seq(dsi, 0x14, 0x41);
	dsi_dcs_write_seq(dsi, 0x15, 0x01);
	dsi_dcs_write_seq(dsi, 0x16, 0x41);
	dsi_dcs_write_seq(dsi, 0x17, 0xff);
	dsi_dcs_write_seq(dsi, 0x18, 0x00);
	dsi_dcs_write_seq(dsi, 0x48, 0x0f);
	dsi_dcs_write_seq(dsi, 0x4d, 0x80);
	dsi_dcs_write_seq(dsi, 0x4e, 0x40);

	/* Switch Page 8 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x08);
	dsi_dcs_write_seq(dsi, 0xe0, 0x40, 0x24, 0x86, 0xc0, 0x05, 0x55, 0x39, 0x60, 0x8d, 0xb1, 0xa9, 0xe6, 0x11, 0x36, 0x5b, 0xea, 0x83, 0xb7, 0xd9, 0x03, 0xff, 0x27, 0x54, 0x89, 0xb5, 0x03, 0xff);
	dsi_dcs_write_seq(dsi, 0xe1, 0x40, 0x24, 0x86, 0xc0, 0x05, 0x55, 0x39, 0x60, 0x8d, 0xb1, 0xa9, 0xe6, 0x11, 0x36, 0x5b, 0xea, 0x83, 0xb7, 0xd9, 0x03, 0xff, 0x27, 0x54, 0x89, 0xb5, 0x03, 0xff);

	/* Switch Page 6 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x06);
	dsi_dcs_write_seq(dsi, 0xd6, 0x85);
	dsi_dcs_write_seq(dsi, 0x27, 0x20);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_DISPLAY_OFF);
	dsi_dcs_write_seq(dsi, 0x2e, 0x01);
	dsi_dcs_write_seq(dsi, 0xc0, 0xf7);
	dsi_dcs_write_seq(dsi, 0xc1, 0x02);
	dsi_dcs_write_seq(dsi, 0xc2, 0x04);

	/* Switch Page 14 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x0e);
	dsi_dcs_write_seq(dsi, 0x00, 0xa0);
	dsi_dcs_write_seq(dsi, 0x01, 0x28);
	dsi_dcs_write_seq(dsi, MIPI_DCS_EXIT_SLEEP_MODE);
	dsi_dcs_write_seq(dsi, 0x13, 0x14);

	/* Switch Page 2 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x02);
	dsi_dcs_write_seq(dsi, 0x40, 0x43);
	dsi_dcs_write_seq(dsi, 0x42, 0x00);
	dsi_dcs_write_seq(dsi, 0x4a, 0x08);
	dsi_dcs_write_seq(dsi, 0x4d, 0x4e);
	dsi_dcs_write_seq(dsi, 0x4e, 0x00);
	dsi_dcs_write_seq(dsi, 0x1a, 0x48);

	/* Switch Page 7 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x07);
	dsi_dcs_write_seq(dsi, 0x0f, 0x02);

	/* Switch Page 0 */
	dsi_dcs_write_seq(dsi, 0xff, 0x98, 0x81, 0x00);
	dsi_dcs_write_seq(dsi, 0x35, 0x00);
	dsi_dcs_write_seq(dsi, 0x36, 0x00);
	dsi_dcs_write_seq(dsi, MIPI_DCS_SET_DISPLAY_ON);
	msleep(20);

	return 0;
}

static int haifei_ili9881h_prepare(struct drm_panel *panel)
{
	struct haifei_ili9881h *ctx = to_haifei_ili9881h(panel);
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

	msleep(30);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(10);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(120);

	ret = haifei_ili9881h_init_sequence(ctx);
	if (ret < 0) {
		dev_err(panel->dev, "Failed to send init sequence: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
		regulator_disable(ctx->supply_neg);
		regulator_disable(ctx->supply_pos);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int haifei_ili9881h_unprepare(struct drm_panel *panel)
{
	struct haifei_ili9881h *ctx = to_haifei_ili9881h(panel);

	if (!ctx->prepared)
		return 0;

	dsi_dcs_write_seq(ctx->dsi, MIPI_DCS_SET_DISPLAY_OFF);
	msleep(10);
	dsi_dcs_write_seq(ctx->dsi, MIPI_DCS_ENTER_SLEEP_MODE);
	msleep(120);

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	regulator_disable(ctx->supply_neg);
	regulator_disable(ctx->supply_pos);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode haifei_ili9881h_default_mode = {
	.clock = 86000,
	.hdisplay = 720,
	.hsync_start = 720 + 40,
	.hsync_end = 720 + 40 + 8,
	.htotal = 720 + 40 + 8 + 38,
	.vdisplay = 1520,
	.vsync_start = 1520 + 246,
	.vsync_end = 1520 + 246 + 4,
	.vtotal = 1520 + 246 + 4 + 8,
	.width_mm = 67,
	.height_mm = 142,
	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static int haifei_ili9881h_get_modes(struct drm_panel *panel)
{
	struct drm_connector *connector = panel->connector;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &haifei_ili9881h_default_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = haifei_ili9881h_default_mode.width_mm;
	connector->display_info.height_mm = haifei_ili9881h_default_mode.height_mm;
	connector->display_info.bpc = 8;

	return 1;
}

static const struct drm_panel_funcs haifei_ili9881h_panel_funcs = {
	.prepare = haifei_ili9881h_prepare,
	.unprepare = haifei_ili9881h_unprepare,
	.get_modes = haifei_ili9881h_get_modes,
};

static int haifei_ili9881h_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct haifei_ili9881h *ctx;
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

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &haifei_ili9881h_panel_funcs;

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		dev_err(dev, "Failed to attach DSI device: %d\n", ret);
		return ret;
	}

	return 0;
}

static int haifei_ili9881h_remove(struct mipi_dsi_device *dsi)
{
	struct haifei_ili9881h *ctx = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id haifei_ili9881h_of_match[] = {
	{ .compatible = "haifei,hjc6217-ili9881h" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, haifei_ili9881h_of_match);

static struct mipi_dsi_driver haifei_ili9881h_driver = {
	.probe = haifei_ili9881h_probe,
	.remove = haifei_ili9881h_remove,
	.driver = {
		.name = "panel-haifei-ili9881h",
		.of_match_table = haifei_ili9881h_of_match,
	},
};
module_mipi_dsi_driver(haifei_ili9881h_driver);

MODULE_AUTHOR("Franklin Kelechi (kelexine) <https://github.com/kelexine>");
MODULE_DESCRIPTION("DRM Panel Driver for Haifei HJC6217 (ILI9881H) 720x1520");
MODULE_LICENSE("GPL");
