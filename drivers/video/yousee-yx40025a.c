// SPDX-License-Identifier: GPL-2.0+
/*
 * Yousee YX40025A 4.0" round 720x720 panel driver for U-Boot
 *
 * The panel uses ST7701S silicon with a vendor-specific (non-standard)
 * initialisation sequence across three register pages. Commands are sent
 * over a 9-bit SPI bus (MIPI DBI type C) where bit 8 distinguishes command
 * (0) from data (1).
 *
 * Copyright (C) 2026 KJ Ngineering
 */

#include <backlight.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <linux/delay.h>
#include <mipi_dbi.h>
#include <panel.h>
#include <power/regulator.h>
#include <spi.h>
#include <asm-generic/gpio.h>
#include <video.h>

/* MIPI DCS commands used during init */
#define YX40025A_SLPOUT		0x11
#define YX40025A_DISPON		0x29
#define YX40025A_COLMOD		0x3A
#define YX40025A_MADCTL		0x36

struct yx40025a_priv {
	struct mipi_dbi	dbi;
	struct spi_slave *spi;
	struct gpio_desc reset;
	struct udevice *backlight;
	struct udevice *supply;
};

/* Send one register write: command byte + one data byte */
static int yx40025a_write(struct yx40025a_priv *priv, u8 reg, u8 val)
{
	return mipi_dbi_command_buf(&priv->dbi, reg, &val, 1);
}

/* Send a bare command with no data */
static int yx40025a_cmd(struct yx40025a_priv *priv, u8 cmd)
{
	return mipi_dbi_command_buf(&priv->dbi, cmd, NULL, 0);
}

/* Page select: three sequential FF writes (vendor protocol) */
static int yx40025a_page(struct yx40025a_priv *priv, u8 page)
{
	int ret;

	ret = yx40025a_write(priv, 0xFF, 0x30); if (ret) return ret;
	ret = yx40025a_write(priv, 0xFF, 0x52); if (ret) return ret;
	ret = yx40025a_write(priv, 0xFF, page);
	return ret;
}

static int yx40025a_init_sequence(struct yx40025a_priv *priv)
{
	int ret;

	/* Reset: assert (active low) → 120ms → deassert → 120ms */
	dm_gpio_set_value(&priv->reset, 1);
	mdelay(120);
	dm_gpio_set_value(&priv->reset, 0);
	mdelay(120);

	ret = yx40025a_cmd(priv, YX40025A_SLPOUT);	if (ret) return ret;
	mdelay(120);

	/* --- Page 01 --- */
	ret = yx40025a_page(priv, 0x01);		if (ret) return ret;

	ret = yx40025a_write(priv, 0xE3, 0x00);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x0A, 0x11);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x23, 0xA0);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x24, 0x32);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x25, 0x12);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x26, 0x2E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x27, 0x2E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x29, 0x02);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x2A, 0xCF);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x32, 0x34);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x38, 0x9C);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x39, 0xA7);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x3A, 0x27);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x3B, 0x94);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x42, 0x6D);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x43, 0x83);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x81, 0x00);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x91, 0x67);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x92, 0x67);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA0, 0x52);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA1, 0x50);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA4, 0x9C);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA7, 0x02);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA8, 0x01);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA9, 0x02);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xAA, 0xA8);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xAB, 0x28);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xAE, 0xD2);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xAF, 0x02);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB0, 0xD2);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB2, 0x26);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB3, 0x26);	if (ret) return ret;

	/* --- Page 02 --- */
	ret = yx40025a_page(priv, 0x02);		if (ret) return ret;

	ret = yx40025a_write(priv, 0xB1, 0x0A);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD1, 0x0E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB4, 0x2F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD4, 0x2D);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB2, 0x0C);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD2, 0x0C);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB3, 0x30);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD3, 0x2A);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB6, 0x1E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD6, 0x16);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB7, 0x3B);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD7, 0x35);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xC1, 0x08);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xE1, 0x08);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB8, 0x0D);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD8, 0x0D);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB9, 0x05);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD9, 0x05);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xBD, 0x15);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xDD, 0x15);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xBC, 0x13);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xDC, 0x13);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xBB, 0x12);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xDB, 0x10);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xBA, 0x11);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xDA, 0x11);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xBE, 0x17);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xDE, 0x17);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xBF, 0x0F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xDF, 0x0F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xC0, 0x16);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xE0, 0x16);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB5, 0x2E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD5, 0x3F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xB0, 0x03);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xD0, 0x02);	if (ret) return ret;

	/* --- Page 03 --- */
	ret = yx40025a_page(priv, 0x03);		if (ret) return ret;

	ret = yx40025a_write(priv, 0x08, 0x09);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x09, 0x0A);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x0A, 0x0B);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x0B, 0x0C);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x28, 0x22);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x2A, 0xE9);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x2B, 0xE9);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x34, 0x51);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x35, 0x01);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x36, 0x26);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x37, 0x13);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x40, 0x07);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x41, 0x08);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x42, 0x09);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x43, 0x0A);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x44, 0x22);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x45, 0xDB);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x46, 0xDC);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x47, 0x22);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x48, 0xDD);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x49, 0xDE);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x50, 0x0B);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x51, 0x0C);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x52, 0x0D);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x53, 0x0E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x54, 0x22);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x55, 0xDF);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x56, 0xE0);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x57, 0x22);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x58, 0xE1);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x59, 0xE2);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x80, 0x1E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x81, 0x1E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x82, 0x1F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x83, 0x1F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x84, 0x05);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x85, 0x0A);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x86, 0x0A);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x87, 0x0C);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x88, 0x0C);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x89, 0x0E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x8A, 0x0E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x8B, 0x10);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x8C, 0x10);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x8D, 0x00);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x8E, 0x00);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x8F, 0x1F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x90, 0x1F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x91, 0x1E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x92, 0x1E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x93, 0x02);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x94, 0x04);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x96, 0x1E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x97, 0x1E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x98, 0x1F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x99, 0x1F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x9A, 0x05);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x9B, 0x09);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x9C, 0x09);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x9D, 0x0B);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x9E, 0x0B);	if (ret) return ret;
	ret = yx40025a_write(priv, 0x9F, 0x0D);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA0, 0x0D);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA1, 0x0F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA2, 0x0F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA3, 0x00);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA4, 0x00);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA5, 0x1F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA6, 0x1F);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA7, 0x1E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA8, 0x1E);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xA9, 0x01);	if (ret) return ret;
	ret = yx40025a_write(priv, 0xAA, 0x03);	if (ret) return ret;

	/* --- Page 00 --- */
	ret = yx40025a_page(priv, 0x00);		if (ret) return ret;

	ret = yx40025a_write(priv, YX40025A_COLMOD, 0x77); if (ret) return ret;
	ret = yx40025a_write(priv, YX40025A_MADCTL, 0x0A); if (ret) return ret;
	ret = yx40025a_cmd(priv,  YX40025A_SLPOUT);	if (ret) return ret;
	mdelay(200);
	ret = yx40025a_cmd(priv,  YX40025A_DISPON);	if (ret) return ret;
	mdelay(200);

	return 0;
}

static int yx40025a_panel_enable_backlight(struct udevice *dev)
{
	struct yx40025a_priv *priv = dev_get_priv(dev);
	int ret;

	if (CONFIG_IS_ENABLED(DM_REGULATOR) && priv->supply) {
		ret = regulator_set_enable(priv->supply, true);
		if (ret) {
			dev_err(dev, "failed to enable supply: %d\n", ret);
			return ret;
		}
	}

	ret = mipi_dbi_spi_init(priv->spi, &priv->dbi, NULL);
	if (ret) {
		dev_err(dev, "MIPI DBI SPI init failed: %d\n", ret);
		return ret;
	}

	ret = yx40025a_init_sequence(priv);
	if (ret) {
		dev_err(dev, "panel init sequence failed: %d\n", ret);
		return ret;
	}

	if (priv->backlight) {
		ret = backlight_enable(priv->backlight);
		if (ret)
			dev_warn(dev, "backlight enable failed: %d\n", ret);
	}

	return 0;
}

static int yx40025a_panel_get_display_timing(struct udevice *dev,
					     struct display_timing *timing)
{
	/*
	 * YX40025A 720x720 @ 39 MHz
	 * h: 720 active + 46 FP + 2 sync + 44 BP = 812 total
	 * v: 720 active + 50 FP + 5 sync + 16 BP = 791 total
	 */
	memset(timing, 0, sizeof(*timing));
	timing->pixelclock.typ	= 39000000;
	timing->hactive.typ	= 720;
	timing->hfront_porch.typ = 46;
	timing->hsync_len.typ	= 2;
	timing->hback_porch.typ	= 44;
	timing->vactive.typ	= 720;
	timing->vfront_porch.typ = 50;
	timing->vsync_len.typ	= 5;
	timing->vback_porch.typ	= 16;
	timing->flags		= DISPLAY_FLAGS_DE_HIGH |
				  DISPLAY_FLAGS_PIXDATA_POSEDGE;
	return 0;
}

static int yx40025a_panel_of_to_plat(struct udevice *dev)
{
	struct yx40025a_priv *priv = dev_get_priv(dev);
	int ret;

	if (CONFIG_IS_ENABLED(DM_REGULATOR)) {
		ret = device_get_supply_regulator(dev, "power-supply",
						  &priv->supply);
		if (ret && ret != -ENOENT) {
			dev_err(dev, "failed to get supply: %d\n", ret);
			return ret;
		}
	}

	ret = gpio_request_by_name(dev, "reset-gpios", 0, &priv->reset,
				   GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "failed to get reset GPIO: %d\n", ret);
		return ret;
	}

	ret = uclass_get_device_by_phandle(UCLASS_PANEL_BACKLIGHT, dev,
					   "backlight", &priv->backlight);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "failed to get backlight: %d\n", ret);
		return ret;
	}

	return 0;
}

static int yx40025a_panel_probe(struct udevice *dev)
{
	struct yx40025a_priv *priv = dev_get_priv(dev);

	priv->spi = dev_get_parent_priv(dev);
	return 0;
}

static const struct panel_ops yx40025a_panel_ops = {
	.enable_backlight	= yx40025a_panel_enable_backlight,
	.get_display_timing	= yx40025a_panel_get_display_timing,
};

static const struct udevice_id yx40025a_panel_ids[] = {
	{ .compatible = "yousee,yx40025a" },
	{ }
};

U_BOOT_DRIVER(yousee_yx40025a) = {
	.name		= "yousee_yx40025a",
	.id		= UCLASS_PANEL,
	.of_match	= yx40025a_panel_ids,
	.ops		= &yx40025a_panel_ops,
	.of_to_plat	= yx40025a_panel_of_to_plat,
	.probe		= yx40025a_panel_probe,
	.priv_auto	= sizeof(struct yx40025a_priv),
};
