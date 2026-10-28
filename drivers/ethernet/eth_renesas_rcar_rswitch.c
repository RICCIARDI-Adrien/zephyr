/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/clock_control.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/phy.h>
//#include <zephyr/pm/device.h>
//#include <zephyr/pm/device_runtime.h>
#include "eth_renesas_rcar_rswitch_priv.h"

LOG_MODULE_REGISTER(eth_rcar_rswitch, CONFIG_ETHERNET_LOG_LEVEL);

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

/* R-Switch Ethernet Agent Mode Configuration register */
#define RSWITCH_TSNA_EAMC 0x0000
/* R-Switch Ethernet Agent Mode Configuration register bits */
#define RSWITCH_TSNA_EAMC_OPC_MASK 0x03
#define RSWITCH_TSNA_EAMC_OPC_RESET 0
#define RSWITCH_TSNA_EAMC_OPC_DISABLE 1
#define RSWITCH_TSNA_EAMC_OPC_CONFIG 2
#define RSWITCH_TSNA_EAMC_OPC_OPERATION 3

/* R-Switch Ethernet Agent Mode Status register */
#define RSWITCH_TSNA_EAMS 0x0004
/* R-Switch Ethernet Agent Mode Status register bits */
#define RSWITCH_TSNA_EAMS_OPS_MASK 0x03
#define RSWITCH_TSNA_EAMS_OPS_RESET 0
#define RSWITCH_TSNA_EAMS_OPS_DISABLE 1
#define RSWITCH_TSNA_EAMS_OPS_CONFIG 2
#define RSWITCH_TSNA_EAMS_OPS_OPERATION 3

/* R-Switch MAC PHY station management register */
#define RSWITCH_RMAC_MPSM 0x0000
/* R-Switch MAC PHY station management register bits */
#define RSWITCH_RMAC_MPSM_PRD_MASK 0x0000FFFF
#define RSWITCH_RMAC_MPSM_PRD_SHIFT 16
#define RSWITCH_RMAC_MPSM_POP_MASK 0x00000003
#define RSWITCH_RMAC_MPSM_POP_SHIFT 13
#define RSWITCH_RMAC_MPSM_POP_C22_WRITE 1
#define RSWITCH_RMAC_MPSM_POP_C22_READ 2
#define RSWITCH_RMAC_MPSM_POP_C45_ADDRESS 0
#define RSWITCH_RMAC_MPSM_POP_C45_WRITE 1
#define RSWITCH_RMAC_MPSM_POP_C45_POST_READ_INCREMENT_ADDRESS 2
#define RSWITCH_RMAC_MPSM_POP_C45_READ 3
#define RSWITCH_RMAC_MPSM_PRA_MASK 0x0000001F
#define RSWITCH_RMAC_MPSM_PRA_SHIFT 8
#define RSWITCH_RMAC_MPSM_PDA_MASK 0x0000001F
#define RSWITCH_RMAC_MPSM_PDA_SHIFT 3
#define RSWITCH_RMAC_MPSM_MFF BIT(2)
#define RSWITCH_RMAC_MPSM_PSME_MASK 0x00000001
#define RSWITCH_RMAC_MPSM_PSME_SHIFT 0
#define RSWITCH_RMAC_MPSM_PSME BIT(0)

/* R-Switch MAC PHY interfaces configuration register */
#define RSWITCH_RMAC_MPIC 0x0004
/* R-Switch MAC PHY interfaces configuration register bits */
#define RSWITCH_RMAC_MPIC_PSMCT_MASK 0x0000000F
#define RSWITCH_RMAC_MPIC_PSMCT_SHIFT 28
#define RSWITCH_RMAC_MPIC_PSMHT_MASK 0x0000000F
#define RSWITCH_RMAC_MPIC_PSMHT_SHIFT 24
#define RSWITCH_RMAC_MPIC_PSMHT_DEFAULT_HOLD_TIME 6
#define RSWITCH_RMAC_MPIC_PSMCS_MASK 0x000000FF
#define RSWITCH_RMAC_MPIC_PSMCS_SHIFT 13
#define RSWITCH_RMAC_MPIC_PLSPP BIT(10)
#define RSWITCH_RMAC_MPIC_PIPP BIT(9)
#define RSWITCH_RMAC_MPIC_LSC_MASK 0x00000007
#define RSWITCH_RMAC_MPIC_LSC_SHIFT 3
#define RSWITCH_RMAC_MPIC_LSC_10_MBPS 0x0
#define RSWITCH_RMAC_MPIC_LSC_100_MBPS 0x1
#define RSWITCH_RMAC_MPIC_LSC_1_GBPS 0x2
#define RSWITCH_RMAC_MPIC_LSC_2_5_GBPS 0x3
#define RSWITCH_RMAC_MPIC_LSC_5_GBPS 0x4
#define RSWITCH_RMAC_MPIC_LSC_10_GBPS 0x5
#define RSWITCH_RMAC_MPIC_PIS_MASK 0x00000007
#define RSWITCH_RMAC_MPIC_PIS_SHIFT 0
#define RSWITCH_RMAC_MPIC_PIS_MII 0
#define RSWITCH_RMAC_MPIC_PIS_GMII 2
#define RSWITCH_RMAC_MPIC_PIS_XGMII 4

#define RSWITCH_TSN_COUNT 13

#define RSWITCH_TSNA_AREA_SIZE 0x1000
#define RSWITCH_RMAC_AREA_SIZE 0x1000

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

enum eth_rswitch_phy_mode {
	ETH_RSWITCH_PHY_MODE_MII,
	ETH_RSWITCH_PHY_MODE_GMII,
	ETH_RSWITCH_PHY_MODE_XGMII,
	ETH_RSWITCH_PHY_MODE_INVALID
};

struct eth_rswitch_tsn_config {
	int id;
	uint32_t tsna_offset; /* Specific to this port */
	uint32_t rmac_offset; /* Specific to this port */
	struct net_eth_mac_config mac_config;
	clock_control_subsys_t tsnes_clk;
	const struct device *global_dev;
	const struct device *phy_dev;
	enum eth_rswitch_phy_mode phy_mode;
};

struct eth_rswitch_tsn_data {
	//bool is_
	enum phy_link_speed current_speed;
};

enum eth_rswitch_operating_mode {
	RSWITCH_OPERATING_MODE_RESET,
	RSWITCH_OPERATING_MODE_DISABLE,
	RSWITCH_OPERATING_MODE_CONFIG,
	RSWITCH_OPERATING_MODE_OPERATION
};

// TODO pareil pour port ?
static inline uint32_t eth_rswitch_read(const struct device *dev, uint32_t offset)
{
	const struct eth_rswitch_config *config = dev->config;

	uint32_t res = sys_read32(DEVICE_MMIO_GET(dev) + offset);
	//printk("\033[32mLECTURE 0x%lX=0x%08X\033[0m\n", DEVICE_MMIO_GET(dev) + offset, res);
	return res;

	// TODO vraie version
	//return sys_read32(DEVICE_MMIO_GET(dev) + offset);
}

// TODO pareil pour port ?
static inline void eth_rswitch_write(const struct device *dev, uint32_t offset, uint32_t value)
{
	const struct eth_rswitch_config *config = dev->config;

	//printk("\033[32mECRITURE MMIO=0x%lX\033[0m\n", DEVICE_MMIO_GET(dev) + offset);

	sys_write32(value, DEVICE_MMIO_GET(dev) + offset);
}

/* Note : this function can sleep, do not use it in interrupt context. */
static inline int eth_rswitch_busy_wait(const struct device *global_dev, uint32_t offset,
					uint32_t bit_mask, uint32_t expected_value)
{
	const uint32_t TIMEOUT = 100000; /* 100 ms */

	if (!WAIT_FOR((eth_rswitch_read(global_dev, offset) & bit_mask) == expected_value, TIMEOUT,
		k_sleep(K_USEC(10)))) {
		LOG_DBG("Busy wait timed out (offset=0x%08X, bit_mask=0x%08X, exp_value=0x%08X).",
			offset, bit_mask, expected_value);
		return -EAGAIN;
	}

	return 0;
}

static int eth_rswitch_set_operating_mode(const struct device *tsn_dev,
					  enum eth_rswitch_operating_mode mode)
{
	const struct eth_rswitch_tsn_config *tsn_config = tsn_dev->config;
	const struct device *global_dev = tsn_config->global_dev;
	uint32_t val;

	/* Basic parameter sanitization */
	mode &= RSWITCH_TSNA_EAMC_OPC_MASK;

	printk("op mode AVANT EAMC=%d, EAMS=%d\n", eth_rswitch_read(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMC), eth_rswitch_read(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMS));

	/* The TSN must be in Disable mode before going to any other mode */
	val = eth_rswitch_read(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMS);
	if ((val & RSWITCH_TSNA_EAMC_OPC_MASK) != RSWITCH_OPERATING_MODE_DISABLE) {
		eth_rswitch_write(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMC,
			RSWITCH_OPERATING_MODE_DISABLE);
		if (eth_rswitch_busy_wait(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMS,
			RSWITCH_TSNA_EAMS_OPS_MASK, RSWITCH_OPERATING_MODE_DISABLE) != 0) {
			LOG_ERR("Failed to set the Disable operating mode for %s.\n",
				tsn_dev->name);
			__ASSERT_NO_MSG(0);
			return -EIO;
		}
	}

	if (mode == RSWITCH_OPERATING_MODE_DISABLE) {
		return 0;
	}
	eth_rswitch_write(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMC, mode);
	if (eth_rswitch_busy_wait(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMS,
		RSWITCH_TSNA_EAMS_OPS_MASK, mode) != 0) {
		LOG_ERR("Failed to set the %d operating mode for %s.\n", mode, tsn_dev->name);
		__ASSERT_NO_MSG(0);
		return -EIO;
	}

printk("op mode APRES EAMC=%d, EAMS=%d\n", eth_rswitch_read(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMC), eth_rswitch_read(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMS));

	// TODO
	//net_eth_carrier_set(data->iface, state->is_up);

	return 0;
}

static int eth_rswitch_mdio_read(const struct device *dev, uint8_t prtad, uint16_t regad,
				 uint8_t devad, uint16_t *data, bool is_c45)
{
	const struct eth_rswitch_tsn_config *tsn_config = dev->config;
	const struct device *global_dev = tsn_config->global_dev;
	uint32_t val = 0, base_offset;

	//printk("{%s:%d} AVANT MPSM=0x%08X MPIC=0x%08X\n", __func__, __LINE__, eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPSM), eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPIC));

	/* Clause 45 frame format */
	if (is_c45) {
	}
	/* Clause 22 frame format */
	else {
		val = RSWITCH_RMAC_MPSM_POP_C22_READ << RSWITCH_RMAC_MPSM_POP_SHIFT |
			(regad & RSWITCH_RMAC_MPSM_PRA_MASK) << RSWITCH_RMAC_MPSM_PRA_SHIFT |
			(prtad & RSWITCH_RMAC_MPSM_PDA_MASK) << RSWITCH_RMAC_MPSM_PDA_SHIFT;

	}
	val |= RSWITCH_RMAC_MPSM_PSME;

	//printk("{%s:%d} val=0x%08X\n", __func__, __LINE__, val);

	base_offset = tsn_config->rmac_offset + RSWITCH_RMAC_MPSM;
	eth_rswitch_write(global_dev, base_offset, val);
	//val |= RSWITCH_RMAC_MPSM_PSME;
	//eth_rswitch_write(global_dev, base_offset, val);
	//printk("{%s:%d} APRES MPSM=0x%08X MPIC=0x%08X\n", __func__, __LINE__, eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPSM), eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPIC));
	if (eth_rswitch_busy_wait(global_dev, base_offset,
		RSWITCH_RMAC_MPSM_PSME_MASK << RSWITCH_RMAC_MPSM_PSME_SHIFT, 0) != 0) {
		LOG_ERR("Failed to transmit the MDIO read frame for device %s "
			"(prtad=0x%X, regad=0x%X, devad=0x%X, is_c45=%d).",
			dev->name, prtad, regad, devad, is_c45);
		printk("{%s:%d} FAILED MPSM=0x%08X MPIC=0x%08X\n", __func__, __LINE__, eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPSM), eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPIC));
		return -EIO;
	}

	/* Retrieve the register data */
	val = eth_rswitch_read(global_dev, base_offset);
	*data = (val >> RSWITCH_RMAC_MPSM_PRD_SHIFT) & RSWITCH_RMAC_MPSM_PRD_MASK;

//printk("\033[32m{%s:%d} FIN OK MPSM=0x%08X MPIC=0x%08X\033[0m\n", __func__, __LINE__, eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPSM), eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPIC));

	return 0;
}

int eth_rswitch_mdio_c22_read(const struct device *dev, uint8_t prtad, uint8_t regad,
			      uint16_t *data)
{
	/*const struct eth_rswitch_tsn_config *tsn_config = dev->config;
	const struct device *global_dev = tsn_config->global_dev;
	uint32_t val;

	// TEST
	int ret;*/

	// TEST
	//LOG_WRN("AVANT EAMC=%d, EAMS=%d", eth_rswitch_read(port_config->global_dev, port_config->tsna_offset + RSWITCH_TSNA_EAMC), eth_rswitch_read(port_config->global_dev, port_config->tsna_offset + RSWITCH_TSNA_EAMS));

	/*eth_rswitch_write(dev, port_config->tsna_offset + RSWITCH_TSNA_EAMC, RSWITCH_TSNA_EAMC_OPC_CONFIG);
	LOG_WRN("APRES ecriture EAMC EAMC=%d, EAMS=%d", eth_rswitch_read(dev, port_config->tsna_offset + RSWITCH_TSNA_EAMC), eth_rswitch_read(dev, port_config->tsna_offset + RSWITCH_TSNA_EAMS));

	ret = eth_rswitch_busy_wait(DEVICE_MMIO_GET(dev) + port_config->tsna_offset + RSWITCH_TSNA_EAMS,
		RSWITCH_TSNA_EAMS_OPS_MASK, RSWITCH_TSNA_EAMS_OPS_CONFIG);
	LOG_WRN("APRES busywait ret=%d", ret);*/

	//LOG_ERR("MERDE ALORS");

	return eth_rswitch_mdio_read(dev, prtad, regad, 0, data, 0);
}

static void eth_rswitch_phy_cb(const struct device *phy, struct phy_link_state *state,
			       void *user_data)
{
	const struct device *tsn_dev = user_data;
	const struct eth_rswitch_tsn_config *tsn_config = tsn_dev->config;
	const struct device *global_dev = tsn_config->global_dev;
	const struct eth_rswitch_config *global_config = global_dev->config;
	uint32_t base_offset, val;
	int ret;
	//struct eth_rswitch_tsn_data *tsn_data = tsn_dev->data;

	//printk("eth_rswitch_phy_cb %s\n MPIC avant=0x%08X\n", tsn_dev->name, eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPIC));

	/* The MPIC register can only be written in Configuration mode */
	if (eth_rswitch_set_operating_mode(tsn_dev, RSWITCH_OPERATING_MODE_CONFIG) != 0) {
		return;
	}

	/*ret = clock_control_get_rate(global_config->clock_dev, tsn_config->tsnes_clk, &val);
	if (ret < 0) {
		LOG_ERR("Failed to get the TSN clock rate for device %s (%d).",
			tsn_dev->name, ret);
		return;
	}

printk("eth_rswitch_phy_cb %sRATE=%u\n", tsn_dev->name, val);*/

	base_offset = tsn_config->rmac_offset + RSWITCH_RMAC_MPIC;

	// TODO définir fréquence clock en fonction du mode
	switch (tsn_config->phy_mode) {
	case ETH_RSWITCH_PHY_MODE_MII:
		val = RSWITCH_RMAC_MPIC_PIS_MII;
		break;
	case ETH_RSWITCH_PHY_MODE_GMII:
		val = RSWITCH_RMAC_MPIC_PIS_GMII;
		break;
	case ETH_RSWITCH_PHY_MODE_XGMII:
		val = RSWITCH_RMAC_MPIC_PIS_XGMII;
		break;
	default:
		LOG_ERR("Unsupported PHY mode for device %s.", tsn_dev->name);
		return;
	}
	val <<= RSWITCH_RMAC_MPIC_PIS_SHIFT;

	// TODO récupération rate clock ne fonctionne pas
	val |= 256UL << RSWITCH_RMAC_MPIC_PSMCS_SHIFT;

	val |= RSWITCH_RMAC_MPIC_PSMHT_DEFAULT_HOLD_TIME << RSWITCH_RMAC_MPIC_PSMHT_SHIFT;

	if (PHY_LINK_IS_SPEED_1000M(state->speed)) {
		val |= RSWITCH_RMAC_MPIC_LSC_1_GBPS << RSWITCH_RMAC_MPIC_LSC_SHIFT;
	}
	else if (PHY_LINK_IS_SPEED_100M(state->speed)) {
		val |= RSWITCH_RMAC_MPIC_LSC_100_MBPS << RSWITCH_RMAC_MPIC_LSC_SHIFT;
	}
	else if (PHY_LINK_IS_SPEED_10M(state->speed)) {
		val |= RSWITCH_RMAC_MPIC_LSC_10_MBPS << RSWITCH_RMAC_MPIC_LSC_SHIFT;
	}
	else {
		LOG_ERR("Unsupported link speed 0x%X.", state->speed);
		// TEST
		//__ASSERT_NO_MSG(0);
		//return;
	}
	eth_rswitch_write(global_dev, base_offset, val);

	// TODO possible version finale
	if (state->is_up) {
		eth_rswitch_set_operating_mode(tsn_dev, RSWITCH_OPERATING_MODE_OPERATION);
	}
	else {
		//eth_rswitch_set_operating_mode(tsn_dev, RSWITCH_OPERATING_MODE_DISABLE);
		// TEST pour le moment
		eth_rswitch_set_operating_mode(tsn_dev, RSWITCH_OPERATING_MODE_OPERATION);
	}

	//printk("eth_rswitch_phy_cb %s\n MPIC apres=0x%08X\n", tsn_dev->name, eth_rswitch_read(global_dev, tsn_config->rmac_offset + RSWITCH_RMAC_MPIC));
}

static void eth_rswitch_iface_init(struct net_if *iface)
{
	const struct device *dev = net_if_get_device(iface);
	const struct eth_rswitch_tsn_config *tsn_config = dev->config;
	const struct device *global_dev = tsn_config->global_dev;
	const struct eth_rswitch_config *global_config = global_dev->config;
	int ret;
	uint32_t base_offset, val;

	// TODO
	printk("INIT IFACE nom=%s, id=%d, tsnaoffs=0x%08X, rmacoffs=0x%08X\n", dev->name, tsn_config->id, tsn_config->tsna_offset, tsn_config->rmac_offset);
	LOG_HEXDUMP_WRN(tsn_config->mac_config.addr, tsn_config->mac_config.addr_len, "MAC=");

	if (clock_control_on(global_config->clock_dev, tsn_config->tsnes_clk) != 0) {
		LOG_ERR("Failed to turn the tsn%d clock on.", tsn_config->id);
		__ASSERT_NO_MSG(0);
		return;
	}

	/* Enable the port clock */
	base_offset = global_config->coma_offset + RSWITCH_COMA_RCEC;
	val = eth_rswitch_read(global_dev, base_offset);
	val |= 1 << tsn_config->id;
	eth_rswitch_write(global_dev, base_offset, val);

#if 0
printk("iface init AVANT EAMC=%d, EAMS=%d\n", eth_rswitch_read(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMC), eth_rswitch_read(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMS));

	/* Switch the TSN to configuration mode (it is already in disable mode after reset) */
	//base_addr = global_dev->
	eth_rswitch_write(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMC,
		RSWITCH_TSNA_EAMC_OPC_CONFIG);
	if (eth_rswitch_busy_wait(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMS,
		RSWITCH_TSNA_EAMS_OPS_MASK, RSWITCH_TSNA_EAMS_OPS_CONFIG) != 0) {
		LOG_ERR("Failed to set the configuration mode for %s.\n", dev->name);
		__ASSERT_NO_MSG(0);
		return;
	}

printk("iface init APRES EAMC=%d, EAMS=%d\n", eth_rswitch_read(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMC), eth_rswitch_read(global_dev, tsn_config->tsna_offset + RSWITCH_TSNA_EAMS));

	/* Configure the PHY link */

#endif

	net_if_set_link_addr(iface, tsn_config->mac_config.addr, tsn_config->mac_config.addr_len,
		NET_LINK_ETHERNET);
	ethernet_init(iface);
	net_if_carrier_off(iface);

	ret = phy_link_callback_set(tsn_config->phy_dev, eth_rswitch_phy_cb, (void *)dev);
	if (ret != 0) {
		LOG_ERR("Failed to register the PHY link callback for %s (%d).", dev->name, ret);
		__ASSERT_NO_MSG(0);
		return;
	}

	printk("INIT IFACE nom=%s OK\n", dev->name);
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

/*
 * An external "port" of the R-Switch (to be connected to a PHY).
 */
#define DT_DRV_COMPAT renesas_rcar_rswitch_tsn

#define ETH_RSWITCH_GET_PHY_MODE(node_id) \
	DT_ENUM_HAS_VALUE(node_id, phy_connection_type, mii) ? ETH_RSWITCH_PHY_MODE_MII : \
	(DT_ENUM_HAS_VALUE(node_id, phy_connection_type, gmii) ? ETH_RSWITCH_PHY_MODE_GMII : \
	(DT_ENUM_HAS_VALUE(node_id, phy_connection_type, xgmii) ? ETH_RSWITCH_PHY_MODE_XGMII : \
	ETH_RSWITCH_PHY_MODE_INVALID))

#define ETH_RSWITCH_TSN_INIT(inst) \
	static const struct eth_rswitch_tsn_config eth_rswitch_tsn_config_##inst = { \
		.id = DT_INST_PROP(inst, id), \
		.tsna_offset = DT_PROP(DT_INST_PARENT(inst), tsna_reg_base_offset) \
			+ DT_INST_PROP(inst, id) * (RSWITCH_TSNA_AREA_SIZE + RSWITCH_RMAC_AREA_SIZE), \
		.rmac_offset = DT_PROP(DT_INST_PARENT(inst), tsna_reg_base_offset) \
			+ DT_INST_PROP(inst, id) * (RSWITCH_TSNA_AREA_SIZE + RSWITCH_RMAC_AREA_SIZE) \
			+ RSWITCH_TSNA_AREA_SIZE, \
		.mac_config = NET_ETH_MAC_DT_CONFIG_INIT(DT_INST(inst, DT_DRV_COMPAT)), \
		.tsnes_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_IDX(inst, 0, name), \
		.global_dev = DEVICE_DT_GET(DT_INST_PARENT(inst)), \
		.phy_dev = DEVICE_DT_GET(DT_INST_PHANDLE(inst, phy_handle)), \
		.phy_mode = ETH_RSWITCH_GET_PHY_MODE(DT_DRV_INST(inst)) \
	}; \
\
	static struct eth_rswitch_tsn_data eth_rswitch_tsn_data_##inst; \
\
	ETH_NET_DEVICE_DT_INST_DEFINE(inst, NULL, NULL, &eth_rswitch_tsn_data_##inst, &eth_rswitch_tsn_config_##inst, CONFIG_ETH_INIT_PRIORITY, &eth_rswitch_ethernet_api, NET_ETH_MTU);

#if 0
	ETH_NET_DEVICE_INIT_INSTANCE(rswitch_tsn_##inst, "eth" STRINGIFY(DT_INST_PROP(inst, id)), DT_INST_PROP(inst, id), \
		NULL, NULL, \
		NULL, /* TODO data */ \
		&eth_rswitch_tsn_config_##inst, \
		CONFIG_ETH_INIT_PRIORITY, &eth_rswitch_ethernet_api, NET_ETH_MTU);
#endif

DT_INST_FOREACH_STATUS_OKAY(ETH_RSWITCH_TSN_INIT);

// TODO virer si inutile
#if 0
#define ETH_RSWITCH_PORT_DEVICE_INIT(inst, n, port_id, clock) \
	static const struct eth_rswitch_port_config eth_rswitch_port_config_##port_id = { \
		.id = port_id, \
		.tsna_offset = DT_INST_PROP(inst, tsna_reg_base_offset) \
			+ port_id * (RSWITCH_TSNA_AREA_SIZE + RSWITCH_RMAC_AREA_SIZE), \
		.rmac_offset = DT_INST_PROP(inst, tsna_reg_base_offset) \
			+ port_id * (RSWITCH_TSNA_AREA_SIZE + RSWITCH_RMAC_AREA_SIZE) \
			+ RSWITCH_TSNA_AREA_SIZE, \
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

// TODO virer si inutile
#define ETH_RSWITCH_DETECT_PORT(inst, port_id, clock) \
	COND_CODE_1(DT_NODE_EXISTS(DT_INST_CHILD(inst, port##port_id)), \
		(ETH_RSWITCH_PORT_DEVICE_INIT(inst, DT_INST_CHILD(inst, port##port_id), port_id, clock)), \
		())

		//(ETH_RSWITCH_PORT_DEVICE_INIT(DT_INST_CHILD(n, port##port_id))), ())
#endif

/*
 * The R-Switch global controller.
 */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT renesas_rcar_rswitch

#define ETH_RSWITCH_INIT(inst) \
	static const struct eth_rswitch_config eth_rswitch_config_##inst = { \
		DEVICE_MMIO_ROM_INIT(DT_DRV_INST(inst)), \
		/*.reg = DT_INST_REG_ADDR(inst), TODO utile ? */ \
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
	/*ETH_RSWITCH_DETECT_PORT(inst, 0, rsw3tsntes0);*/ \
	/*ETH_RSWITCH_DETECT_PORT(inst, 1, rsw3tsntes1);*/ \
	/*ETH_RSWITCH_DETECT_PORT(inst, 2, rsw3tsntes2);*/ \
	/*ETH_RSWITCH_DETECT_PORT(inst, 3, rsw3tsntes3);*/ \
	/*ETH_RSWITCH_DETECT_PORT(inst, 4, rsw3tsntes4);*/ \
	/*ETH_RSWITCH_DETECT_PORT(inst, 5, rsw3tsntes5);*/ \
	/*ETH_RSWITCH_DETECT_PORT(inst, 6, rsw3tsntes6);*/ \
	/*ETH_RSWITCH_DETECT_PORT(inst, 7, rsw3tsntes7);*/ \
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
