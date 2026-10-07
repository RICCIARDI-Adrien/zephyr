/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>

LOG_MODULE_REGISTER(eth_rcar_rswitch, CONFIG_ETHERNET_LOG_LEVEL);

#define DT_DRV_COMPAT renesas_rcar_rswitch

/* The total amount of TSN IP blocks. */
//#define ETH_RSWITCH_TSN_COUNT 8

/* R-Switch IP Version register */
#define RSWITCH_COMA_RIPV 0x0000

/* R-Switch Reset Configuration register */
#define RSWITCH_COMA_RRC 0x0004
/* R-Switch Reset Configuration register bits */
#define RSWITCH_COMA_RRC_RR BIT(0)

/* R-Switch Clock Enable Configuration register */
#define RSWITCH_COMA_RCEC 0x0008
/* R-Switch Clock Enable Configuration register bits */
#define RSWITCH_COMA_RCEC_RCE BIT(16)

struct eth_rswitch_config {
	DEVICE_MMIO_ROM; /* Must be first */
	//const struct eth_rswitch_port_config *port_configs;
	//size_t ports_count;
	uint32_t reg;
	uint32_t coma_offset;
	const struct device *clock_dev;
	clock_control_subsys_t rsw3_clk;
	clock_control_subsys_t rsw3_tsn_global_clk;
	clock_control_subsys_t rsw3_aes_clk;
	//clock_control_subsys_t rsw3_tsn_clks[ETH_RSWITCH_TSN_COUNT];
	clock_control_subsys_t rsw3_mfwd_clk;
};

struct eth_rswitch_data {
	DEVICE_MMIO_RAM;
};

struct eth_rswitch_port_config {
	int id;
	struct net_eth_mac_config mac_config;
	clock_control_subsys_t tsn_clk;
	const struct device *global_dev;
};

// TODO pareil pour port ?
static inline uint32_t eth_rswitch_read(const struct device *dev, uint32_t offset)
{
	const struct eth_rswitch_config *config = dev->config;

	LOG_ERR("LECTURE ADDR 0x%08X MMIO=0x%lX", config->reg + offset, DEVICE_MMIO_GET(dev) + offset);

	return sys_read32(/*config->reg*/DEVICE_MMIO_GET(dev) + offset);
}

// TODO pareil pour port ?
static inline void eth_rswitch_write(const struct device *dev, uint32_t offset, uint32_t value)
{
	const struct eth_rswitch_config *config = dev->config;

	LOG_ERR("ECRITURE ADDR 0x%08X MMIO=0x%lX", config->reg + offset, DEVICE_MMIO_GET(dev) + offset);

	sys_write32(value, /*config->reg*/DEVICE_MMIO_GET(dev) + offset);
}

//#define rswitch_port_INIT

static void eth_rswitch_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	const struct eth_rswitch_port_config *port_config = dev->config;
	const struct eth_rswitch_config *global_config = port_config->global_dev->config;

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

static const struct ethernet_api eth_rswitch_ethernet_api = {
	.iface_api.init = eth_rswitch_iface_init,
};

static int eth_rswitch_device_init(const struct device *dev)
{
	const struct eth_rswitch_config *config = dev->config;
	int ret;

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
		LOG_ERR("Failed to turn the rsw3_aes clock on.");
		return ret;
	}

	ret = clock_control_on(config->clock_dev, config->rsw3_mfwd_clk);
	if (ret != 0) {
		LOG_ERR("Failed to turn the rsw3_mfwd clock on.");
		return ret;
	}

	DEVICE_MMIO_MAP(dev, K_MEM_CACHE_NONE | K_MEM_DIRECT_MAP);

	/*LOG_WRN("on pd %d", pm_device_on_power_domain(dev));

	pm_device_init_off(dev);
	ret = pm_device_runtime_enable(dev);
		if (ret) {
			LOG_ERR("Failed to enable device runtime PM");
			return ret;
		}

		ret = pm_device_runtime_get(dev);
		if (ret) {
			LOG_ERR("Failed to get device runtime PM state");
			return ret;
		}*/

	/* Reset the controller (must be de-asserted quickly to avoid a power consumption peak) */
	eth_rswitch_write(dev, config->coma_offset + RSWITCH_COMA_RRC, RSWITCH_COMA_RRC_RR);
	eth_rswitch_write(dev, config->coma_offset + RSWITCH_COMA_RRC, 0);

	/* Internally enable the controller clocks (each port will enable its own clock later) */
	eth_rswitch_write(dev, config->coma_offset + RSWITCH_COMA_RCEC, RSWITCH_COMA_RCEC_RCE);

	LOG_DBG("R-Switch controller IPs version: 0x%08X.", eth_rswitch_read(dev, config->coma_offset + RSWITCH_COMA_RIPV));

	return 0;
}

#define ETH_RSWITCH_PORT_DEVICE_INIT(inst, n, port_id, clock) \
	static const struct eth_rswitch_port_config eth_rswitch_port_config_##port_id = { \
		.id = port_id, \
		.mac_config = NET_ETH_MAC_DT_CONFIG_INIT(n), /*DT_PROP(DT_INST_CHILD(n, port##port_id), local_mac_address)*/ \
		.tsn_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, clock, name), \
		.global_dev = DEVICE_DT_GET(DT_DRV_INST(inst)) \
	}; \
\
	ETH_NET_DEVICE_INIT_INSTANCE(rswitch_port_##port_id, "eth" STRINGIFY(port_id), port_id, \
		NULL, NULL, \
		NULL, /* TODO data */ \
		&eth_rswitch_port_config_##port_id, \
		CONFIG_ETH_INIT_PRIORITY, &eth_rswitch_ethernet_api, NET_ETH_MTU);

#define ETH_RSWITCH_DETECT_PORT(inst, port_id, clock) \
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, port##port_id)), \
		(ETH_RSWITCH_PORT_DEVICE_INIT(inst, DT_INST_CHILD(inst, port##port_id), port_id, clock)), \
		())

		//(ETH_RSWITCH_PORT_DEVICE_INIT(DT_INST_CHILD(n, port##port_id))), ())


#define ETH_RSWITCH_INIT(inst) \
	static const struct eth_rswitch_config eth_rswitch_config_##inst = { \
		DEVICE_MMIO_ROM_INIT(DT_DRV_INST(inst)), \
		.reg = DT_INST_REG_ADDR(inst), /* TODO utile ? */ \
		.coma_offset = DT_INST_PROP(inst, coma_reg_offset), \
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(inst)), \
		.rsw3_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, rsw3, name), \
		.rsw3_tsn_global_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, rsw3tsn, name), \
		.rsw3_aes_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, rsw3aes, name), \
		.rsw3_mfwd_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, rsw3mfwd, name) \
	}; \
\
	static struct eth_rswitch_data eth_rswitch_data_##inst; \
\
	/*DT_FOREACH_CHILD(DT_INST_CHILD(n, ports), ETH_RSWITCH_PORT_DEVICE_INIT);*/ \
\
	ETH_RSWITCH_DETECT_PORT(inst, 0, rsw3tsntes0); \
	ETH_RSWITCH_DETECT_PORT(inst, 1, rsw3tsntes1); \
	ETH_RSWITCH_DETECT_PORT(inst, 2, rsw3tsntes2); \
	ETH_RSWITCH_DETECT_PORT(inst, 3, rsw3tsntes3); \
	ETH_RSWITCH_DETECT_PORT(inst, 4, rsw3tsntes4); \
	ETH_RSWITCH_DETECT_PORT(inst, 5, rsw3tsntes5); \
	ETH_RSWITCH_DETECT_PORT(inst, 6, rsw3tsntes6); \
	ETH_RSWITCH_DETECT_PORT(inst, 7, rsw3tsntes7); \
\
	DEVICE_DT_INST_DEFINE(inst, eth_rswitch_device_init, NULL,					\
		&eth_rswitch_data_##inst, &eth_rswitch_config_##inst,				\
		POST_KERNEL, CONFIG_ETH_INIT_PRIORITY, NULL);


	/*ETH_NET_DEVICE_INIT_INSTANCE(n,  \*/
	/*ETH_NET_DEVICE_DT_INST_DEFINE(n, ETH_RSWITCH_INIT, NULL, \
		NULL*/ /* DATA TODO *//*, \
		eth_rswitch_config_##n, \
		CONFIG_ETH_INIT_PRIORITY, \
		&eth_rswitch_ethernet_api, \
		NET_ETH_MTU);*/


DT_INST_FOREACH_STATUS_OKAY(ETH_RSWITCH_INIT);
