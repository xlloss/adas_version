/*
 * Copy From OmniVision ov10635 sensor camera driver
 *
 * Copyright (C) 2015-2017 Cogent Embedded, Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
 
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/videodev2.h>

#include <media/soc_camera.h>
#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <linux/of_graph.h>
#include <linux/fs.h> 
#include <linux/file.h> 
#include <linux/mm.h> 
#include <asm/segment.h> 
#include <asm/uaccess.h>
#include "tp2854.h"

struct tp2854sub_priv {
	struct v4l2_subdev sd;
	struct v4l2_ctrl_handler hdl;
	struct media_pad pad;
	struct v4l2_rect rect;
	struct v4l2_rect crop_rect;
	int fps_denominator;
	int init_complete;
    int video_mode;
	u8 id[6];
};

static inline struct tp2854sub_priv *to_tp2854(const struct i2c_client *client)
{
	return container_of(i2c_get_clientdata(client), struct tp2854sub_priv, sd);
}

static int tp2854_read_reg(struct i2c_client *client ,unsigned char reg  , unsigned char *val)
{
	int ret;
	int tmpaddr ;

	tmpaddr = client->addr;
	client->addr = TP2854_ADDR;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev,
			"read fail: chip 0x%x register 0x%x: %d\n",
			client->addr, reg, ret);
	} else {
		*val = ret;
	}
 	client->addr = tmpaddr;
	return ret < 0 ? ret : 0;
}


static int tp2854_write_reg(struct i2c_client *client ,unsigned char reg, unsigned char val)
{
	int ret;
	int tmpaddr ;

	tmpaddr = client->addr;
	client->addr = TP2854_ADDR; 
	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		dev_err(&client->dev,
			"%s:write reg error:reg=%2x,val=%2x\n", __func__,
			reg, val);
		client->addr = tmpaddr;
		return -1;
	}
	client->addr = tmpaddr;
	return 0;
}


static int tp2854_g_mbus_config(struct v4l2_subdev *sd,
				 struct v4l2_mbus_config *cfg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//dev_info(&client->dev,  "tp2854_g_mbus_config addr %x\n",client->addr);

	if (client->addr == TP2854_SUBCAM_0_FAKE_SLAV) {
         /* one lane , channel 0 */
        cfg->flags = V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_0 |
        V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	} else if (client->addr == TP2854_SUBCAM_1_FAKE_SLAV) {
        /* one lane , channel 1 */
        cfg->flags = V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_1 |
        V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	} else if (client->addr == TP2854_SUBCAM_2_FAKE_SLAV) {
        /* one lane , channel 2 */
        cfg->flags = V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_2 |
        V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	} else if (client->addr == TP2854_SUBCAM_3_FAKE_SLAV) {
        /* one lane , channel 3 */
        cfg->flags = V4L2_MBUS_CSI2_1_LANE | V4L2_MBUS_CSI2_CHANNEL_3 |
        V4L2_MBUS_CSI2_CONTINUOUS_CLOCK;
	}
	cfg->type = V4L2_MBUS_CSI2;
	return 0;
}


static int tp2854_s_stream(struct v4l2_subdev *sd, int enable)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	//dev_info(&client->dev,  "tp2854_s_stream\n");
	return 0;
}

static int tp2854_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_captureparm *cp = &parms->parm.capture;

	//dev_info(&client->dev,  "tp2854_s_parm\n");

    //if not video capture , return error
	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	if (cp->extendedmode != 0)
		return -EINVAL;

	return 0;
}

static int tp2854_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct v4l2_captureparm *cp = &parms->parm.capture;
    struct tp2854sub_priv *priv = i2c_get_clientdata(client);

	//dev_info(&client->dev,  "tp2854_g_parm\n");

	if (parms->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	memset(cp, 0, sizeof(struct v4l2_captureparm));

	//cp->capability = V4L2_CAP_TIMEPERFRAME; not support

    /*
     * timeperframe.discrete.denominator / timeperframe.discrete.numerator
     */

	cp->timeperframe.numerator = 1;
	cp->timeperframe.denominator = priv->fps_denominator;

	return 0;
}


static int tp2854sub_get_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct tp2854sub_priv *priv = i2c_get_clientdata(client);

//	dev_info(&client->dev,  "tp2854sub_get_fmt\n");

	if (format->pad)
		return -EINVAL;

	mf->width = priv->rect.width;
	mf->height = priv->rect.height;
	mf->code = MEDIA_BUS_FMT_YUYV8_2X8; 
	mf->colorspace = V4L2_COLORSPACE_SMPTE170M;
	mf->field = V4L2_FIELD_NONE; //Progressive mode

	return 0;
}

static int tp2854sub_set_fmt(struct v4l2_subdev *sd,
			   struct v4l2_subdev_pad_config *cfg,
			   struct v4l2_subdev_format *format)
{
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

//	dev_info(&client->dev, "tp2854sub_set_fmt\n");

	mf->code = MEDIA_BUS_FMT_YUYV8_2X8;
	mf->colorspace = V4L2_COLORSPACE_SMPTE170M;
	mf->field = V4L2_FIELD_NONE;
	
	if (format->which == V4L2_SUBDEV_FORMAT_TRY)
		cfg->try_fmt = *mf;

	return 0;
}

static int tp2854sub_get_selection(struct v4l2_subdev *sd,
    struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_selection *sel)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct tp2854sub_priv *priv = to_tp2854(client);

	dev_info(&client->dev, "tp2854sub_get_selection\n");
    
	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE)
		return -EINVAL;

	switch (sel->target) {
	case V4L2_SEL_TGT_CROP_BOUNDS:
        pr_err("V4L2_SEL_TGT_CROP_BOUNDS\r\n");
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = priv->crop_rect.width;
		sel->r.height = priv->crop_rect.height;
		return 0;

	case V4L2_SEL_TGT_CROP_DEFAULT:
        pr_err("V4L2_SEL_TGT_CROP_DEFAULT\r\n");
		sel->r.left = 0;
		sel->r.top = 0;
		sel->r.width = priv->crop_rect.width;
		sel->r.height = priv->crop_rect.height;
		return 0;

	case V4L2_SEL_TGT_CROP:
        pr_err("V4L2_SEL_TGT_CROP\r\n");
		sel->r = priv->crop_rect;
		return 0;

	default:
		return -EINVAL;
	}
}


static int tp2854sub_set_selection(struct v4l2_subdev *sd,
    struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_selection *sel)
{
	struct v4l2_rect *rect = &sel->r;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
    struct tp2854sub_priv *priv = to_tp2854(client);

	dev_info(&client->dev,  "tp2854sub_set_selection\n");

	if (sel->which != V4L2_SUBDEV_FORMAT_ACTIVE ||
	    sel->target != V4L2_SEL_TGT_CROP)
		return -EINVAL;

//	rect->left = ALIGN(rect->left, 2);
//	rect->top = ALIGN(rect->top, 2);
//	rect->width = ALIGN(rect->width, 2);
//	rect->height = ALIGN(rect->height, 2);
    pr_err("rect->left %d\r\n", rect->left);
    pr_err("rect->width %d\r\n", rect->width);
    pr_err("rect->top %d\r\n", rect->top);
    pr_err("rect->height %d\r\n", rect->height);
	if ((rect->left + rect->width > priv->rect.width) ||
	    (rect->top + rect->height > priv->rect.height))
		*rect = priv->rect;

//	rect->left = 0;
//	rect->top = 0;
//	rect->width = priv->rect.width;
//	rect->height = priv->rect.height;

	priv->crop_rect.left = rect->left;
	priv->crop_rect.top = rect->top;
	priv->crop_rect.width = rect->width;
	priv->crop_rect.height = rect->height;

	return 0;
}


static int tp2854sub_enum_mbus_code(struct v4l2_subdev *sd,
    struct v4l2_subdev_pad_config *cfg, struct v4l2_subdev_mbus_code_enum *code)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	if (code->pad || code->index > 0)
		return -EINVAL;

//	dev_info(&client->dev,  "tp2854sub_enum_mbus_code\n");
	code->code = MEDIA_BUS_FMT_YUYV8_2X8;
	return 0;
}


static int tp2854sub_s_power(struct v4l2_subdev *sd, int on)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

    dev_info(&client->dev,  "%s on = %d\n", __func__, on);
	if (on) {
		tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_ENABLE);
		tp2854_write_reg(client ,TP2854_MIPI_STOPCLK, MIPI_CLK_NORMAL);
		tp2854_write_reg(client ,TP2854_MIPI_OUTPUT_EN, ENABLE_ALL_LANES);
        tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_DISABLE);
		
	} else {
		tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_ENABLE);
		tp2854_write_reg(client ,TP2854_MIPI_STOPCLK, MIPI_CLK_STOP);
		tp2854_write_reg(client ,TP2854_MIPI_OUTPUT_EN, DISABLE_ALL_LANES);
        tp2854_write_reg(client ,TP2854_PAGE, MIPI_PAGE_DISABLE);
	}
    msleep(50);
	return 0;
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int tp2854sub_g_register(struct v4l2_subdev *sd,
				      struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret = 0;
	u8 val = 0;

	ret = tp2854_read_reg(client, (u8)reg->reg, &val);
	if (ret < 0)
		return ret;

	reg->val = val;
	reg->size = sizeof(u8);

	return 0;
}

static int tp2854sub_s_register(struct v4l2_subdev *sd,
				      const struct v4l2_dbg_register *reg)
{
	/* struct i2c_client *client = v4l2_get_subdevdata(sd); */

	return 0;
}
#endif

static struct v4l2_subdev_core_ops tp2854sub_core_ops = {
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = tp2854sub_g_register,
	.s_register = tp2854sub_s_register,
#endif
	.s_power = tp2854sub_s_power,
};



static const struct v4l2_subdev_pad_ops tp2854sub_subdev_pad_ops = {
	.enum_mbus_code = tp2854sub_enum_mbus_code,
	.set_selection = tp2854sub_set_selection,
	.get_selection = tp2854sub_get_selection,
	.set_fmt = tp2854sub_set_fmt,
	.get_fmt = tp2854sub_get_fmt,
};

static const struct v4l2_subdev_video_ops tp2854sub_video_ops = {
	.s_stream = tp2854_s_stream,
	.g_mbus_config = tp2854_g_mbus_config,
	.g_parm = tp2854_g_parm,
	.s_parm = tp2854_s_parm,
};

static struct v4l2_subdev_ops tp2854_subdev_ops = {
	.video = &tp2854sub_video_ops,
	.pad = &tp2854sub_subdev_pad_ops,
	.core = &tp2854sub_core_ops,
};

static const struct i2c_device_id tp2854sub_id[] = {
	{ "tp2854sub", 0 },
	{ }
};

int tp2854sub_check_deviceId(struct i2c_client *client)
{
	u8 val = 0; 

	/* read TP2854_ID_1 */
	tp2854_read_reg(client, TP2854_ID1_REG, &val);
	dev_info(&client->dev,  "tp2854 read 0xfe %x\n", val);
	if (val != TP2854_ID_1) {
		dev_info(&client->dev,"dismatch tp2854 device Id 1");
		return -1;
	}

    /* read TP2854_ID_2 */
	tp2854_read_reg(client, TP2854_ID2_REG, &val);
	dev_info(&client->dev,  "tp2854 read 0xff %x\n", val);
	if (val != TP2854_ID_2) {
		dev_info(&client->dev,"dismatch tp2854 device Id 1");
		return -1;
	}

    return 1;
}


static int tp2854sub_parse_dt(struct i2c_client *client)
{
    struct device_node *np = client->dev.of_node;
    struct device_node *endpoint = NULL, *rendpoint = NULL;
    struct device_node *p_endpoint = NULL, *np_endpoint = NULL;
    int i, nxpl = 0;
    struct tp2854sub_priv *priv = i2c_get_clientdata(client);

    for (i = 0; ; i++) {
        endpoint = of_graph_get_next_endpoint(np, endpoint);
        if (!endpoint)
            break;

        rendpoint = of_parse_phandle(endpoint, "remote-endpoint", 0);
        if (rendpoint) {
            p_endpoint = of_get_next_parent(rendpoint);
            if (!p_endpoint)
                continue;

            np_endpoint = of_get_next_parent(p_endpoint);
            if (!np_endpoint) 
                continue;

            if (!of_property_read_u32(np_endpoint, "npxl", &nxpl)) {
                priv->video_mode = nxpl;
                return;
            }
        }
    }
}

static void tp2854sub_video_mode(struct tp2854sub_priv *priv)
{
  switch (priv->video_mode) {
        case NPXL_720P_60:
            priv->rect.left = 0;
            priv->rect.top = 0;
            priv->rect.width = 1280;
            priv->rect.height = 720;
            priv->fps_denominator = 30;
            break;

        case NPXL_720P_50:
            priv->rect.left = 0;
            priv->rect.top = 0;
            priv->rect.width = 1280;
            priv->rect.height = 720;
            priv->fps_denominator = 50;
            break;

        case NPXL_720P_30:
            pr_err("%s USE NPXL_720P_30\r\n", __func__);
            priv->rect.left = 0;
            priv->rect.top = 0;
            priv->rect.width = 1280;
            priv->rect.height = 720;
            priv->fps_denominator = 30;
            break;

        case NPXL_720P_25:
            priv->rect.left = 0;
            priv->rect.top = 0;
            priv->rect.width = 1280;
            priv->rect.height = 720;
            priv->fps_denominator = 25;
            break;

        case NPXL_1080P_30:
            pr_err("%s USE NPXL_1080P_30\r\n", __func__);
            priv->rect.left = 0;
            priv->rect.top = 0;
            priv->rect.width = 1920;
            priv->rect.height = 1080;
            priv->fps_denominator = 30;
            break;

        case NPXL_1080P_25:
            priv->rect.left = 0;
            priv->rect.top = 0;
            priv->rect.width = 1920;
            priv->rect.height = 1080;
            priv->fps_denominator = 25;
            break;

        case NPXL_480I:
            break;

        case NPXL_576I:
            break;

        default:
        break;
    };
    priv->crop_rect = priv->rect;
}

static int tp2854sub_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
    struct tp2854sub_priv *priv;
	int ret;

	dev_info(&client->dev, "tp2854sub_probe \n");

	//check if tp2854 exist
	if (tp2854sub_check_deviceId(client) < 0)
		return 0;


	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	//initialize I2C
	v4l2_i2c_subdev_init(&priv->sd, client, &tp2854_subdev_ops);
	priv->sd.flags = V4L2_SUBDEV_FL_HAS_DEVNODE;

    /*
     *   priv->rect.width = TP2854_MAX_WIDTH;
     *   priv->rect.height = TP2854_MAX_HEIGHT;
     *   priv->fps_denominator = 30;
     */

    tp2854sub_parse_dt(client);
    tp2854sub_video_mode(priv);
	priv->pad.flags = MEDIA_PAD_FL_SOURCE;
	priv->sd.entity.flags |= MEDIA_ENT_F_CAM_SENSOR;
    priv->sd.host_priv = priv;
	ret = media_entity_pads_init(&priv->sd.entity, 1, &priv->pad);
	if (ret < 0)
		goto cleanup;
	ret = v4l2_async_register_subdev(&priv->sd);
	if (ret)
		goto cleanup;

	priv->init_complete = 1;

	return 0;

cleanup:
	media_entity_cleanup(&priv->sd.entity);
	v4l2_ctrl_handler_free(&priv->hdl);
	v4l2_device_unregister_subdev(&priv->sd);

	return ret;
}


static int tp2854sub_remove(struct i2c_client *client)
{
	struct tp2854sub_priv *priv = i2c_get_clientdata(client);

	v4l2_async_unregister_subdev(&priv->sd);
	media_entity_cleanup(&priv->sd.entity);
	v4l2_ctrl_handler_free(&priv->hdl);
	v4l2_device_unregister_subdev(&priv->sd);
	return 0;
}


MODULE_DEVICE_TABLE(i2c, tp2854sub_id);

static const struct of_device_id tp2854sub_of_ids[] = {
	{ .compatible = "tp,tp2864_sub_cam", },
	{ }
};

static struct i2c_driver tp2854sub_i2c_driver = {
	.driver	= {
		.name		= "tp2854sub",
		.of_match_table	= tp2854sub_of_ids,
	},
	.probe		= tp2854sub_probe,
	.remove		= tp2854sub_remove,
	.id_table	= tp2854sub_id,
};

module_i2c_driver(tp2854sub_i2c_driver);

MODULE_DESCRIPTION("TP2854 SUB_CAM Driver");
MODULE_AUTHOR("FuckMe");
MODULE_LICENSE("GPL");

