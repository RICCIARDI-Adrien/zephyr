/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/mdio.h>
#include <zephyr/logging/log.h>
#include "../eth_renesas_rcar_rswitch_priv.h"

LOG_MODULE_REGISTER(mdio_rswitch, CONFIG_MDIO_LOG_LEVEL);

#define DT_DRV_COMPAT renesas_rcar_rswitch_mdio

struct mdio_rswitch_config {
	const struct device *global_dev;
};

static int mdio_rswitch_c22_read(const struct device *dev, uint8_t prtad, uint8_t devad,
				 uint16_t *data)
{
	// TEST
	LOG_ERR("mdio_rswitch_c22_read prtad=%d, devad=%d", prtad, devad);

	const struct mdio_rswitch_config *config = dev->config;

	return eth_rswitch_mdio_c22_read(config->global_dev, prtad, devad, data);
}

static int mdio_rswitch_c22_write(const struct device *dev, uint8_t prtad, uint8_t devad,
				  uint16_t data)
{
	/*const struct mdio_lan865x_config *const cfg = dev->config;

	return eth_lan865x_mdio_c22_write(cfg->dev, prtad, regad, data);*/
	LOG_ERR("mdio_rswitch_c22_read MERDE");
	return 0;
}

static DEVICE_API(mdio, mdio_rswitch_api) = {
	.read = mdio_rswitch_c22_read,
	.write = mdio_rswitch_c22_write,
	/*.read_c45 = mdio_rswitch_c45_read,
	.write_c45 = mdio_rswitch_c45_write*/
};

static int mdio_rswitch_init(const struct device *dev)
{

}

#define MDIO_RSWITCH_INIT(inst) \
	static struct mdio_rswitch_config mdio_rswitch_config_##inst = { \
		.global_dev = DEVICE_DT_GET(DT_INST_PARENT(inst)) \
	}; \
\
	DEVICE_DT_INST_DEFINE(inst, NULL, NULL, NULL, &mdio_rswitch_config_##inst, POST_KERNEL, \
		CONFIG_MDIO_INIT_PRIORITY, &mdio_rswitch_api);

DT_INST_FOREACH_STATUS_OKAY(MDIO_RSWITCH_INIT)
