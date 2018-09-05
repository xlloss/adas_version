/*
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include "tp2854sub.c"
#include <linux/fs.h> 
#include <linux/i2c.h>

static enum {
	ID_TP2854,
} chip_id;

static int tpxx_probe(struct i2c_client *client,
			 const struct i2c_device_id *did)
{
    int ret;
    chip_id = -EINVAL;

    ret = tp2854sub_probe(client, did);
    if (!ret) {
        chip_id = ID_TP2854;
        goto out;
    }

	v4l_err(client, "failed to probe @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);
out:
	return ret;
}

static int tpxx_remove(struct i2c_client *client)
{
    switch (chip_id) {

    case ID_TP2854:
        tp2854sub_remove(client);
        break;
    };

	return 0;
}

static const struct i2c_device_id tpxx_id[] = {
	{ "tp2854_sub_cam", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tpxx_id);

static const struct of_device_id tpxx_of_ids[] = {
	{ .compatible = "tp,tp2854_sub_cam", },
	{ }
};
MODULE_DEVICE_TABLE(of, tpxx_of_ids);

static struct i2c_driver tpxx_i2c_driver = {
	.driver	= {
		.name		= "tp2854_sub_cam",
		.of_match_table	= tpxx_of_ids,
	},
	.probe		= tpxx_probe,
	.remove		= tpxx_remove,
	.id_table	= tpxx_id,
};

module_i2c_driver(tpxx_i2c_driver);

MODULE_DESCRIPTION("TP Generic Driver");
MODULE_AUTHOR("Regulus");
MODULE_LICENSE("GPL");
