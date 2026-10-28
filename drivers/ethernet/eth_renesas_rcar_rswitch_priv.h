/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_ETH_RENESAS_RCAR_RSWITCH_PRIV_H_
#define ZEPHYR_DRIVERS_ETH_RENESAS_RCAR_RSWITCH_PRIV_H_

#if 0
#include <zephyr/drivers/clock_control.h>
#include <zephyr/net/ethernet.h>

struct eth_rswitch_port_config {
	int id;
	uint32_t tsna_offset; /* Specific to this port */
	uint32_t rmac_offset; /* Specific to this port */
	struct net_eth_mac_config mac_config;
	clock_control_subsys_t tsn_clk;
	const struct device *global_dev;
};
#endif

#include <zephyr/device.h>

/*int eth_rswitch_mdio_c22_read(const struct device *dev, uint8_t port_address, uint8_t reg_address,
			      uint16_t *data);*/

int eth_rswitch_mdio_read(const struct device *tsn_dev, uint8_t prtad, uint16_t regad,
			  uint8_t devad, uint16_t *data, bool is_c45);

int eth_rswitch_mdio_write(const struct device *tsn_dev, uint8_t prtad, uint8_t regad,
			   uint8_t devad, uint16_t data, bool is_c45);

#endif /* ZEPHYR_DRIVERS_ETH_RENESAS_RCAR_RSWITCH_PRIV_H_ */
