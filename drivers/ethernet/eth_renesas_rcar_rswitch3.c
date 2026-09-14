/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/ethernet.h>

LOG_MODULE_REGISTER(eth_rcar_rswitch3, CONFIG_ETHERNET_LOG_LEVEL);

#define DT_DRV_COMPAT renesas_rcar_rswitch3

/* The total amount of TSN IP blocks. */
#define ETH_RSWITCH3_TSN_COUNT 8

struct eth_rswitch3_config {
	//const struct eth_rswitch3_port_config *port_configs;
	//size_t ports_count;
	const struct device *clock_dev;
	clock_control_subsys_t rsw3_clk;
	clock_control_subsys_t rsw3_tsn_global_clk;
	clock_control_subsys_t rsw3_aes_clk;
	//clock_control_subsys_t rsw3_tsn_clks[ETH_RSWITCH3_TSN_COUNT];
	clock_control_subsys_t rsw3_mfwd_clk;
};

struct eth_rswitch3_port_config {
	int id;
	struct net_eth_mac_config mac_config;
	clock_control_subsys_t tsn_clk;
	const struct device *global_dev;
};

//#define RSWITCH3_PORT_INIT

static void eth_rswitch3_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	const struct eth_rswitch3_port_config *port_config = dev->config;
	const struct eth_rswitch3_config *global_config = port_config->global_dev->config;

	// TODO
	LOG_WRN("INIT IFACE nom=%s, id=%d", dev->name, port_config->id);
	LOG_HEXDUMP_WRN(port_config->mac_config.addr, port_config->mac_config.addr_len, "MAC=");

	if (clock_control_on(global_config->clock_dev, port_config->tsn_clk) != 0) {
		LOG_ERR("Failed to turn the tsn%d clock on.", port_config->id);
		__ASSERT_NO_MSG();
	}

	net_if_set_link_addr(iface, port_config->mac_config.addr, port_config->mac_config.addr_len,
		NET_LINK_ETHERNET);
	ethernet_init(iface);
	net_if_carrier_off(iface);

	LOG_WRN("INIT IFACE nom=%s OK", dev->name);
}

static const struct ethernet_api eth_rswitch3_ethernet_api = {
	.iface_api.init = eth_rswitch3_iface_init,
};

static int eth_rswitch3_device_init(const struct device *dev)
{
	const struct eth_rswitch3_config *config = dev->config;
	int ret;

	LOG_ERR("CACA START...");

	if (!device_is_ready(config->clock_dev)) {
		LOG_ERR("The clock device is not ready.");
		return -ENODEV;
	}

	ret = clock_control_on(config->clock_dev, config->rsw3_clk);
	if (ret != 0) {
		LOG_ERR("Failed to turn the rsw3 clock on.");
		return ret;
	}

	ret = clock_control_on(config->clock_dev, config->rsw3_tsn_global_clk);
	if (ret != 0) {
		LOG_ERR("Failed to turn the rsw3_tsn_global clock on.");
		return ret;
	}

	ret = clock_control_on(config->clock_dev, config->rsw3_aes_clk);
	if (ret != 0) {
		LOG_ERR("Failed to turn the rsw3_aesk clock on.");
		return ret;
	}

	ret = clock_control_on(config->clock_dev, config->rsw3_mfwd_clk);
	if (ret != 0) {
		LOG_ERR("Failed to turn the rsw3_mfwd clock on.");
		return ret;
	}

	LOG_ERR("CACA OK !");

	// TODO log dbg avec version IP (si le registre existe)

	return 0;
}

#define ETH_RSWITCH3_PORT_DEVICE_INIT(inst, n, port_id, clock) \
	static const struct eth_rswitch3_port_config eth_rswitch3_port_config_##port_id = { \
		.id = port_id, \
		.mac_config = NET_ETH_MAC_DT_CONFIG_INIT(n), /*DT_PROP(DT_INST_CHILD(n, port##port_id), local_mac_address)*/ \
		.tsn_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, clock, name), \
		.global_dev = DEVICE_DT_GET(DT_DRV_INST(inst)) \
	}; \
\
	ETH_NET_DEVICE_INIT_INSTANCE(rswitch3_port_##port_id, "eth" STRINGIFY(port_id), port_id, \
		NULL, NULL, \
		NULL, /* TODO data */ \
		&eth_rswitch3_port_config_##port_id, \
		CONFIG_ETH_INIT_PRIORITY, &eth_rswitch3_ethernet_api, NET_ETH_MTU);

#define ETH_RSWITCH3_DETECT_PORT(inst, port_id, clock) \
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, port##port_id)), \
		(ETH_RSWITCH3_PORT_DEVICE_INIT(inst, DT_INST_CHILD(inst, port##port_id), port_id, clock)), \
		())

		//(ETH_RSWITCH3_PORT_DEVICE_INIT(DT_INST_CHILD(n, port##port_id))), ())


#define ETH_RSWITCH3_INIT(inst) \
	static const struct eth_rswitch3_config eth_rswitch3_config_##inst = { \
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(inst)), \
		.rsw3_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, rsw3, name), \
		.rsw3_tsn_global_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, rsw3tsn, name), \
		.rsw3_aes_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, rsw3aes, name), \
		.rsw3_mfwd_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, rsw3mfwd, name) \
	}; \
\
	/*DT_FOREACH_CHILD(DT_INST_CHILD(n, ports), ETH_RSWITCH3_PORT_DEVICE_INIT);*/ \
\
	ETH_RSWITCH3_DETECT_PORT(inst, 0, rsw3tsntes0); \
	ETH_RSWITCH3_DETECT_PORT(inst, 1, rsw3tsntes1); \
	ETH_RSWITCH3_DETECT_PORT(inst, 2, rsw3tsntes2); \
	ETH_RSWITCH3_DETECT_PORT(inst, 3, rsw3tsntes3); \
	ETH_RSWITCH3_DETECT_PORT(inst, 4, rsw3tsntes4); \
	ETH_RSWITCH3_DETECT_PORT(inst, 5, rsw3tsntes5); \
	ETH_RSWITCH3_DETECT_PORT(inst, 6, rsw3tsntes6); \
	ETH_RSWITCH3_DETECT_PORT(inst, 7, rsw3tsntes7); \
\
	DEVICE_DT_INST_DEFINE(inst, eth_rswitch3_device_init, NULL,					\
		NULL, &eth_rswitch3_config_##inst,				\
		POST_KERNEL, CONFIG_ETH_INIT_PRIORITY, NULL);


	/*ETH_NET_DEVICE_INIT_INSTANCE(n,  \*/
	/*ETH_NET_DEVICE_DT_INST_DEFINE(n, eth_rswitch3_init, NULL, \
		NULL*/ /* DATA TODO *//*, \
		eth_rswitch3_config_##n, \
		CONFIG_ETH_INIT_PRIORITY, \
		&eth_rswitch3_ethernet_api, \
		NET_ETH_MTU);*/


DT_INST_FOREACH_STATUS_OKAY(ETH_RSWITCH3_INIT);
