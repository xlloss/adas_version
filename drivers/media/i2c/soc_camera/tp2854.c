/*
 * TECHPOINT HD-TVI tp2854 driver
 * (efer to MAXIM max9286 GMSL driver)
 *
 * Copyright (C) 2015-2018 Cogent Embedded, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of_graph.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/videodev2.h>
#include <linux/fs.h> 
#include <linux/file.h> 
#include <linux/mm.h> 
#include <asm/segment.h>
#include <asm/uaccess.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include "tp2854.h"

struct tp2854_priv {
	struct v4l2_subdev sd[4];
	struct device_node *sd_of_node[4];
	int lanes;
	int csi_rate;
	int hsync;
	int vsync;
    int nxpl;
    int tvi_clk;
	atomic_t use_count;
	struct i2c_client*client;
};

struct i2c_client g_client ;

static int tp2854_read_reg(struct i2c_client *client ,
    unsigned char reg  , unsigned char *val)
{
	int ret;
    
	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
        dev_err(&client->dev,
            "read fail: chip 0x%x reg 0x%x", client->addr, reg);
        return ret;
    }

	dev_info(&client->dev, "addr 0x%x reg 0x%x value 0x%x\n",
        client->addr, reg, ret);

    *val = ret;

	return ret;
}


static int tp2854_write_reg(struct i2c_client *client,
    unsigned char reg, unsigned char val)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		dev_info(&client->dev,
			"%s:write reg error:reg=%2x,val=%2x\n", __func__,
			reg, val);
		return -1;
	}
	return 0;
}

int tp2854_hardware_init(struct i2c_client *client)
{
    struct tp2854_priv *priv = i2c_get_clientdata(client);

	dev_info(&client->dev,"tp2854_hardware_init\n");

    /* disable MIPI register access */
	tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_DISABLE);

     /* This seems to need Y first */
     /*  tp2854_write_reg(client ,0x02, 0xc2); */

    pr_err("%s priv->nxpl %d\r\n", __func__, priv->nxpl);
    switch (priv->nxpl) {
        case NPXL_720P_60:
            priv->hsync = 1280;
            priv->vsync = 720;

            if (priv->tvi_clk == TP2854_TVP_CLK_148M) {
                tp2854_write_reg(client ,TP2854_NPXL_H, 0x0C);
                tp2854_write_reg(client ,TP2854_NPXL_L, 0xE4);

            } else {
                tp2854_write_reg(client ,TP2854_NPXL_H, 0x06);
                tp2854_write_reg(client ,TP2854_NPXL_L, 0x72);
            }

            tp2854_write_reg(client ,TP2854_OUT_H_DELAY_H, 0x13);
            tp2854_write_reg(client ,TP2854_OUT_H_DELAY_L, 0x15);
            tp2854_write_reg(client ,TP2854_OUT_V_DELAY_L, 0x19);
            break;

        case NPXL_720P_50:
            priv->hsync = 1280;
            priv->vsync = 720;
            break;

        case NPXL_720P_30:
            pr_err("%s USE NPXL_720P_30\r\n", __func__);
            priv->hsync = 1280;
            priv->vsync = 720;

            if (priv->tvi_clk == TP2854_TVP_CLK_148M) {
                tp2854_write_reg(client ,TP2854_NPXL_H, 0x0C);
                tp2854_write_reg(client ,TP2854_NPXL_L, 0xE4);

            } else {
                tp2854_write_reg(client ,TP2854_NPXL_H, 0x06);
                tp2854_write_reg(client ,TP2854_NPXL_L, 0x72);
            }


            tp2854_write_reg(client ,TP2854_OUT_H_DELAY_H, 0x13);
            tp2854_write_reg(client ,TP2854_OUT_H_DELAY_L, 0x15);
            tp2854_write_reg(client ,TP2854_OUT_V_DELAY_L, 0x19);
            break;

        case NPXL_720P_25:
            priv->hsync = 1280;
            priv->vsync = 720;
            break;

        case NPXL_1080P_30:
            pr_err("%s USE NPXL_1080P_30\r\n", __func__);
            priv->hsync = 1920;
            priv->vsync = 1080;
            tp2854_write_reg(client ,TP2854_NPXL_H, 0x08);
            tp2854_write_reg(client ,TP2854_NPXL_L, 0x98);
            tp2854_write_reg(client ,TP2854_OUT_H_DELAY_H, 0x03);
            tp2854_write_reg(client ,TP2854_OUT_H_DELAY_L, 0xd2); 
            tp2854_write_reg(client ,TP2854_OUT_V_DELAY_L, 0x29);
            break;

        case NPXL_1080P_25:
            priv->hsync = 1920;
            priv->vsync = 1080;
            break;

        case NPXL_480I:
            break;

        case NPXL_576I:
            break;

        default:
        break;
    };

    if (priv->tvi_clk == TP2854_TVP_CLK_148M)
        tp2854_write_reg(client ,TP2854_MISC_CTL, 0x05 & ~FSL_74MHZ_148MHZ_SYS_CLK);
    else
        tp2854_write_reg(client ,TP2854_MISC_CTL, 0x05 | FSL_74MHZ_148MHZ_SYS_CLK);

    if (priv->vsync == 720) {
        /* 8bitYUV Y first, BT656 720p HD mode */
        tp2854_write_reg(client ,TP2854_DECODE_CTRL, 0xc2);
        tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_ENABLE);
        tp2854_write_reg(client ,0x13, 0x24);
        tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_DISABLE);
    } else if (priv->vsync == 1080) {
        tp2854_write_reg(client ,TP2854_DECODE_CTRL, 0xc0);
        tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_ENABLE);
        tp2854_write_reg(client ,0x13, 0x04);
        tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_DISABLE);
    } else {
        pr_err("Don't know what format ? priv->vsync %d\r\n", priv->vsync);
    }

    tp2854_write_reg(client ,TP2854_OUT_H_ACTIVE_L, priv->hsync & 0xFF); 
    tp2854_write_reg(client ,TP2854_OUT_V_ACTIVE_L, priv->vsync & 0xFF);
    tp2854_write_reg(client ,TP2854_OUT_V_H_ACTIVE_H,
        (((priv->vsync & 0x0F00) >> 4) | 
        ((priv->hsync & 0x0F00) >> 8)));


	tp2854_write_reg(client ,TP2854_CLK_DATA_OUT, 0x00);
	tp2854_write_reg(client ,TP2854_SYS_CLK_CTRL, 0xf0);
	tp2854_write_reg(client ,TP2854_COL_H_PLL_FR_CTL, 0x34);

    if (priv->tvi_clk == TP2854_TVP_CLK_148M)
        tp2854_write_reg(client ,TP2854_EQ1_HYSTER, 0x43 & ~EQ_CLK_FSEL);
    else
        tp2854_write_reg(client ,TP2854_EQ1_HYSTER, 0x43 | EQ_CLK_FSEL);

    /* enable MIPI register access */
	tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_ENABLE);

    if (priv->tvi_clk == TP2854_TVP_CLK_148M)
        tp2854_write_reg(client ,TP2854_MIPI_PLL_CTRL4, 0x24 & ~OUT_DIV_EN);
    else
        tp2854_write_reg(client ,TP2854_MIPI_PLL_CTRL4, 0x24 | OUT_DIV_EN);

	tp2854_write_reg(client ,TP2854_MIPI_CLK_LAEN_CTRL, 0xf8);

	tp2854_write_reg(client ,TP2854_MIPI_CLK_EN, 0x01);
	tp2854_write_reg(client ,TP2854_MIPI_OUTPUT_EN, 0x0);
	tp2854_write_reg(client ,TP2854_MIPI_PLL_CTRL5, 0x04);
	tp2854_write_reg(client ,TP2854_MIPI_PLL_CTRL6, 0x00);

	tp2854_write_reg(client ,TP2854_MIPI_NUM_LAN,
        NUM_CHANNELS(priv->lanes) | NUM_LANES(priv->lanes));

	tp2854_write_reg(client ,TP2854_MIPI_VERTUAL_CHID, 0x1b);

    /* disable CSi2 */ 
	tp2854_write_reg(client ,TP2854_MIPI_STOPCLK, 0x02);
	dev_info(&client->dev , "disable MIPI CSI 2\n");

    /* disable MIPI register access */
	tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_DISABLE);

	return 0;
}

static int tp2854_initialize(struct i2c_client *client)
{
    dev_info(&client->dev,  "leave tp2854_initialize\n");
    tp2854_hardware_init(client);
	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int tp2854_g_register(struct v4l2_subdev *sd,
				      struct v4l2_dbg_register *reg)
{
	int ret;
	u8 val = 0;

	ret = tp2854_read_reg(&g_client, reg->reg, &val);
	if (ret < 0)
		return ret;

	reg->val = val;
	reg->size = sizeof(u8);

	return 0;
}

static int tp2854_s_register(struct v4l2_subdev *sd,
				      const struct v4l2_dbg_register *reg)
{
    return tp2854_write_reg( &g_client, (u8)reg->reg, (u8)reg->val);
}
#endif

static int tp2854_s_power(struct v4l2_subdev *sd, int on)
{
	/* 
    struct tp2854_priv *priv = v4l2_get_subdevdata(sd);
    struct i2c_client *client = priv->client;
	dev_info(&client->dev,  "tp2854_s_power %d\n", on);
    */

	return 0;
}

static int tp2854_registered_async(struct v4l2_subdev *sd)
{
	return 0;
}

static struct v4l2_subdev_core_ops tp2854_subdev_core_ops = {
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = tp2854_g_register,
	.s_register = tp2854_s_register,
#endif
	.s_power = tp2854_s_power,
	.registered_async = tp2854_registered_async,
};

static struct v4l2_subdev_ops tp2854_subdev_ops = {
	.core = &tp2854_subdev_core_ops,
};

static int tp2854_parse_dt(struct i2c_client *client)
{
	struct tp2854_priv *priv = i2c_get_clientdata(client);
	struct device_node *np = client->dev.of_node;
    //struct device *dev = &client->dev;
	u8 val = 0;

    /* read TP2854_ID_1 */
	tp2854_read_reg(client, TP2854_ID1_REG, &val);
	if (val != TP2854_ID_1) {
        dev_info(&client->dev, "tp2854 read TP2854_ID1 fail %x\n", val);
		return -ENODEV;
	}
    dev_info(&client->dev,  "tp2854 read TP2854_ID1 %x\n", val);

    /* read TP2854_ID_2 */
	tp2854_read_reg(client, TP2854_ID2_REG, &val);
	if (val != TP2854_ID_2) {
        dev_info(&client->dev,  "tp2854 read TP2854_ID2 fail %x\n", val);
		return -ENODEV;
	}
    dev_info(&client->dev,  "tp2854 read TP2854_ID2 %x\n", val);


	if (of_property_read_u32(np, "npxl", &priv->nxpl))
    	priv->nxpl = 2; /* 720P  30 frame, npxl = 2 */

	if (of_property_read_u32(np, "lanes", &priv->lanes))
		priv->lanes = 4;

	if (of_property_read_u32(np, "tvi-clk", &priv->tvi_clk))
		priv->tvi_clk = TP2854_TVP_CLK_74M;

    /*
	if (of_property_read_u32(np, "techpoint,hsync", &priv->hsync))
		priv->hsync = 0;

	if (of_property_read_u32(np, "tecgpoint,vsync", &priv->vsync))
		priv->vsync = 0;
    */

	return 0;
}

static void tp2854_setup_remote_endpoint(struct i2c_client *client)
{
}


static int tp2854_probe(struct i2c_client *client,
				 const struct i2c_device_id *did)
{
	struct tp2854_priv *priv;
	int err, i;

	dev_info(&client->dev, "%s ver 1.1\n", __func__);
	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	i2c_set_clientdata(client, priv);
	priv->client = client;
	atomic_set(&priv->use_count, 0);

	err = tp2854_parse_dt(client);
	if (err) {
		dev_info(&client->dev, "tp2854_parse_dt error\n");
		goto out;
    }

	memcpy(&g_client ,client ,sizeof(struct i2c_client));
	err = tp2854_initialize(client);
	if (err < 0)
		goto out;

	tp2854_setup_remote_endpoint(client);

	//To perform v4l2 async. register for 4 sub-device
	for (i = 0; i < CHANNEL_NUM; i++) {
		v4l2_subdev_init(&priv->sd[i], &tp2854_subdev_ops);
		priv->sd[i].owner = client->dev.driver->owner;
		priv->sd[i].dev = &client->dev;
		priv->sd[i].grp_id = i;
		v4l2_set_subdevdata(&priv->sd[i], priv);

		snprintf(priv->sd[i].name, V4L2_SUBDEV_NAME_SIZE, "%s.%d %d-%04x",
			 client->dev.driver->name, i, i2c_adapter_id(client->adapter),
			 client->addr);

		err = v4l2_async_register_subdev(&priv->sd[i]);
		dev_info(&client->dev,  "v4l2_async_register_subdev ret %d",err);
		if (err < 0)
			goto out;
	}

out:
	return err;
}

static int tp2854_remove(struct i2c_client *client)
{
	struct tp2854_priv *priv = i2c_get_clientdata(client);
	int i;

	for (i = 0; i < CHANNEL_NUM; i++) {
		v4l2_async_unregister_subdev(&priv->sd[i]);
		v4l2_device_unregister_subdev(&priv->sd[i]);
	}

	return 0;
}

static const struct of_device_id tp2854_dt_ids[] = {
	{ .compatible = "techpoint,tp2854" },
	{},
};
MODULE_DEVICE_TABLE(of, tp2854_dt_ids);

static const struct i2c_device_id tp2854_id[] = {
	{ "tp2854",0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tp2854_id);

static struct i2c_driver tp2854_i2c_driver = {
	.driver	= {
		.name = "tp2854",
		.of_match_table = of_match_ptr(tp2854_dt_ids),
	},
	.probe = tp2854_probe,
	.remove = tp2854_remove,
	.id_table = tp2854_id,
};

module_i2c_driver(tp2854_i2c_driver);

MODULE_DESCRIPTION("HD-TVI driver for tp2854");
MODULE_AUTHOR("Alex Hwang @ Regulus");
MODULE_LICENSE("GPL");
