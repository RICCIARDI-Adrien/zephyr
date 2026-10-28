/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/mdio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>
#include "../eth_renesas_rcar_rswitch_priv.h"

LOG_MODULE_REGISTER(mdio_rswitch, CONFIG_MDIO_LOG_LEVEL);

#define DT_DRV_COMPAT renesas_rcar_rswitch_mdio

struct mdio_rswitch_config {
	const struct device *tsn_dev;
	const struct pinctrl_dev_config *pcfg;
};

static int mdio_rswitch_c22_read(const struct device *dev, uint8_t prtad, uint8_t devad,
				 uint16_t *data)
{
	const struct mdio_rswitch_config *config = dev->config;

	// TEST
	LOG_ERR("mdio_rswitch_c22_read prtad=%d, devad=%d", prtad, devad);

	//return eth_rswitch_mdio_c22_read(config->tsn_dev, prtad, devad, data);
	return eth_rswitch_mdio_read(config->tsn_dev, prtad, devad, 0, data, 0);
}

static int mdio_rswitch_c22_write(const struct device *dev, uint8_t prtad, uint8_t devad,
				  uint16_t data)
{
	const struct mdio_rswitch_config *config = dev->config;

	// TEST
	LOG_ERR("mdio_rswitch_c22_write prtad=%d, devad=%d, data=0x%04X", prtad, devad, data);

	return eth_rswitch_mdio_write(config->tsn_dev, prtad, devad, 0, data, 0);
}

static DEVICE_API(mdio, mdio_rswitch_api) = {
	.read = mdio_rswitch_c22_read,
	.write = mdio_rswitch_c22_write,
	/*.read_c45 = mdio_rswitch_c45_read,
	.write_c45 = mdio_rswitch_c45_write*/
};

static int mdio_rswitch_init(const struct device *dev)
{
	const struct mdio_rswitch_config *config = dev->config;
	int ret;

	ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		LOG_ERR("Failed to apply the pinctrl default state to device %s (%d).",
			dev->name, ret);
		return ret;
	}

	return 0;
}

#define MDIO_RSWITCH_INIT(inst) \
	PINCTRL_DT_INST_DEFINE(inst); \
\
	static struct mdio_rswitch_config mdio_rswitch_config_##inst = { \
		.tsn_dev = DEVICE_DT_GET(DT_INST_PARENT(inst)), \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(inst) \
	}; \
\
	DEVICE_DT_INST_DEFINE(inst, mdio_rswitch_init, NULL, NULL, &mdio_rswitch_config_##inst, POST_KERNEL, \
		CONFIG_MDIO_INIT_PRIORITY, &mdio_rswitch_api);

DT_INST_FOREACH_STATUS_OKAY(MDIO_RSWITCH_INIT)
