/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
#include <zephyr/net/ethernet.h>

LOG_MODULE_REGISTER(eth_rcar_rswitch3, CONFIG_ETHERNET_LOG_LEVEL);

#define DT_DRV_COMPAT renesas_rcar_rswitch3

struct eth_rswitch3_port_config {
	int id;
	struct net_eth_mac_config mac_config;
};

struct eth_rswitch3_config {
	const struct eth_rswitch3_port_config *port_configs;
	size_t ports_count;
};

//#define RSWITCH3_PORT_INIT

static void eth_rswitch3_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	const struct eth_rswitch3_port_config *port_config = dev->config;

	// TODO
	LOG_WRN("INIT IFACE nom=%s, id=%d", dev->name, port_config->id);
	LOG_HEXDUMP_WRN(port_config->mac_config.addr, port_config->mac_config.addr_len, "MAC=");

	//ethernet_init(iface);

	//net_eth_carrier_on(iface);
}

static const struct ethernet_api eth_rswitch3_ethernet_api = {
	.iface_api.init = eth_rswitch3_iface_init,
};

static int eth_rswitch3_device_init(const struct device *dev)
{
	const struct eth_rswitch3_config *config = dev->config;

	LOG_ERR("CACA, nbr ports=%d", config->ports_count);

	return 0;
}

#define ETH_RSWITCH3_PORT_CONFIG_INIT(n) \
	{ \
		.id = 6 \
	},

#define ETH_RSWITCH3_PORT_DEVICE_INIT(n) \
	static const struct eth_rswitch3_port_config eth_rswitch3_port_config_##n = { \
		.id = DT_PROP(n, id), \
		.mac_config = NET_ETH_MAC_DT_CONFIG_INIT(n) \
	}; \
\
	ETH_NET_DEVICE_INIT_INSTANCE(rswitch3_port_##n, "eth" STRINGIFY(DT_PROP(n, id)), n, \
		NULL, NULL, \
		NULL, /* TODO data */ \
		&eth_rswitch3_port_config_##n, \
		CONFIG_ETH_INIT_PRIORITY, &eth_rswitch3_ethernet_api, NET_ETH_MTU);

#if 0
	ETH_NET_DEVICE_DT_INST_DEFINE(n, NULL, NULL, \
		NULL /* DATA TODO */, \
		NULL, /*cfg*/ \
		CONFIG_ETH_INIT_PRIORITY, \
		&eth_rswitch3_ethernet_api, \
		NET_ETH_MTU)
#endif

#define ETH_RSWITCH3_INIT(n) \
	static const struct eth_rswitch3_port_config eth_rswitch3_port_configs_##n[] = { \
		DT_FOREACH_CHILD(DT_INST_CHILD(n, ports), ETH_RSWITCH3_PORT_CONFIG_INIT) \
	}; \
\
	static const struct eth_rswitch3_config eth_rswitch3_config_##n = { \
		.port_configs = eth_rswitch3_port_configs_##n, \
		.ports_count = ARRAY_SIZE(eth_rswitch3_port_configs_##n) \
	}; \
\
	DT_FOREACH_CHILD(DT_INST_CHILD(n, ports), ETH_RSWITCH3_PORT_DEVICE_INIT); \
\
	DEVICE_DT_INST_DEFINE(n, eth_rswitch3_device_init, NULL,					\
		NULL, &eth_rswitch3_config_##n,				\
		POST_KERNEL, CONFIG_ETH_INIT_PRIORITY, NULL);


	/*ETH_NET_DEVICE_INIT_INSTANCE(n,  \*/
	/*ETH_NET_DEVICE_DT_INST_DEFINE(n, eth_rswitch3_init, NULL, \
		NULL*/ /* DATA TODO *//*, \
		eth_rswitch3_config_##n, \
		CONFIG_ETH_INIT_PRIORITY, \
		&eth_rswitch3_ethernet_api, \
		NET_ETH_MTU);*/


DT_INST_FOREACH_STATUS_OKAY(ETH_RSWITCH3_INIT);
