/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/can.h>
#include <zephyr/drivers/can/transceiver.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>

LOG_MODULE_REGISTER(can_rcar_rscanfd, CONFIG_CAN_LOG_LEVEL);

#define DT_DRV_COMPAT renesas_rcar_rscanfd

// TODO faire "enum" par canal ?

// TODO MMIO

/* Channel N Nominal Bitrate Configuration Register */
#define RSCANFD_CFDCNNCFG 0x0000
/* Channel N Nominal Bitrate Configuration Register bits */
#define RSCANFD_CFDCNNCFG_NBRP_MASK 0x3FF
#define RSCANFD_CFDCNNCFG_NBRP_SHIFT 0
#define RSCANFD_CFDCNNCFG_NSJW_MASK 0x07F
#define RSCANFD_CFDCNNCFG_NSJW_SHIFT 10
#define RSCANFD_CFDCNNCFG_NTSEG1_MASK 0xFF
#define RSCANFD_CFDCNNCFG_NTSEG1_SHIFT 17
#define RSCANFD_CFDCNNCFG_NTSEG2_MASK 0xFF
#define RSCANFD_CFDCNNCFG_NTSEG2_SHIFT 25

/* Channel n Control Register */
#define RSCANFD_CFDCNCTR 0x0004
/* Channel n Control Register bits */
#define RSCANFD_CFDCNCTR_CTMS_MASK 0x03
#define RSCANFD_CFDCNCTR_CTMS_SHIFT 25
#define RSCANFD_CFDCNCTR_CTMS_LISTEN_ONLY_MODE 0x01
#define RSCANFD_CFDCNCTR_CTMS_INTERNAL_LOOPBACK_MODE 0x03
#define RSCANFD_CFDCNCTR_CTME_MASK 0x01
#define RSCANFD_CFDCNCTR_CTME_SHIFT 24
#define RSCANFD_CFDCNCTR_CTME_DISABLED 0
#define RSCANFD_CFDCNCTR_CTME_ENABLED 1
#define RSCANFD_CFDCNCTR_CHMDC_MASK 0x03
#define RSCANFD_CFDCNCTR_CHMDC_SHIFT 0
#define RSCANFD_CFDCNCTR_CHMDC_CHANNEL_OPERATION_REQUEST 0
#define RSCANFD_CFDCNCTR_CHMDC_CHANNEL_RESET_REQUEST 0x01
#define RSCANFD_CFDCNCTR_CHMDC_CHANNEL_HALT_REQUEST 0x02

/* Channel n Status Register */
#define RSCANFD_CFDCNSTS 0x0008
/* Channel n Status Register bits */
#define RSCANFD_CFDCNSTS_CSLPSTS BIT(2)
#define RSCANFD_CFDCNSTS_CHLTSTS BIT(1)
#define RSCANFD_CFDCNSTS_CRSTSTS BIT(0)

/* Global IP Version Register */
#define RSCANFD_CFDGIPV 0x0080

/* Global Control Register */
#define RSCANFD_CFDGCTR 0x0088
/* Global Control Register bits */
#define RSCANFD_CFDGCTR_GMDC_MASK 0x03
#define RSCANFD_CFDGCTR_GMDC_SHIFT 0
#define RSCANFD_CFDGCTR_GMDC_GLOBAL_OPERATION_MODE_REQUEST 0x00
#define RSCANFD_CFDGCTR_GMDC_GLOBAL_RESET_MODE_REQUEST 0x01

/* Global Status Register */
#define RSCANFD_CFDGSTS 0x008C
/* Global Status Register bits */
#define RSCANFD_CFDGSTS_GRAMINIT BIT(3)
#define RSCANFD_CFDGSTS_GRSTSTS BIT(0)

/* Global Acceptance Filter List Entry Control Register */
#define RSCANFD_CFDGAFLECTR 0x0098
#define RSCANFD_CFDGAFLECTR_AFLDAE BIT(8)

/* Global Acceptance Filter List Configuration Register n */
#define RSCANFD_CFDGAFLCFGN 0x009C
/* Global Acceptance Filter List Configuration Register n bits */
#define RSCANFD_CFDGAFLCFGN_RNC_CHANNEL_MASK 0x001F
#define RSCANFD_CFDGAFLCFGN_RNC_CHANNEL_EVEN_SHIFT 16
#define RSCANFD_CFDGAFLCFGN_RNC_CHANNEL_ODD_SHIFT 0

/* RX Message Buffer Number Register */
#define RSCANFD_CFDRMNB 0x00AC

/* RX FIFO Configuration / Control Register N */
#define RSCANFD_CFDRFCCN 0x00C0
/* RX FIFO Configuration / Control Register N bits */
#define RSCANFD_CFDRFCCN_RFIM_SHIFT 12
#define RSCANFD_CFDRFCCN_RFIM_EVERY_MESSAGE 1
#define RSCANFD_CFDRFCCN_RFDC_MASK 0x07
#define RSCANFD_CFDRFCCN_RFDC_SHIFT 8
#define RSCANFD_CFDRFCCN_RFDC_DEPTH_64 0x06
#define RSCANFD_CFDRFCCN_RFDC_DEPTH_128 0x07
#define RSCANFD_CFDRFCCN_RFPLS_MASK 0x07
#define RSCANFD_CFDRFCCN_RFPLS_SHIFT 4
#define RSCANFD_CFDRFCCN_RFPLS_SIZE_64 0x07
#define RSCANFD_CFDRFCCN_RFIE_SHIFT 1
#define RSCANFD_CFDRFCCN_RFIE_ENABLE 1
#define RSCANFD_CFDRFCCN_RFE_MASK 0x01
#define RSCANFD_CFDRFCCN_RFE_SHIFT 0
#define RSCANFD_CFDRFCCN_RFE_DISABLE 0
#define RSCANFD_CFDRFCCN_RFE_ENABLE 1

/* Common FIFO Configuration / Control Register N */
#define RSCANFD_CFDCFCCN 0x0120
/* Common FIFO Configuration / Control Register N bits */
#define RSCANFD_CFDCFCCN_CFDC_MASK 0x03
#define RSCANFD_CFDCFCCN_CFDC_SHIFT 21
#define RSCANFD_CFDCFCCN_CFDC_DEPTH_64 0x06
#define RSCANFD_CFDCFCCN_CFDC_DEPTH_128 0x07
#define RSCANFD_CFDCFCCN_CFIM_SHIFT 12
#define RSCANFD_CFDCFCCN_CFIM_EVERY_MESSAGE 1
#define RSCANFD_CFDCFCCN_CFIM_LAST_MESSAGE 0 // TODO moins d'interrupts mais gestion queue de tx callbacks, voir plus tard pour perfs
#define RSCANFD_CFDCFCCN_CFM_MASK 0x03
#define RSCANFD_CFDCFCCN_CFM_SHIFT 8
#define RSCANFD_CFDCFCCN_CFM_RX 0x00
#define RSCANFD_CFDCFCCN_CFM_TX 0x01
#define RSCANFD_CFDCFCCN_CFPLS_SHIFT 4
#define RSCANFD_CFDCFCCN_CFPLS_SIZE_64 0x07
#define RSCANFD_CFDCFCCN_CFTXIE_SHIFT 2
#define RSCANFD_CFDCFCCN_CFTXIE_INT_ENABLED 1
#define RSCANFD_CFDCFCCN_CFRXIE_SHIFT 1
#define RSCANFD_CFDCFCCN_CFRXIE_INT_ENABLED 1
#define RSCANFD_CFDCFCCN_CFE_MASK 0x01
#define RSCANFD_CFDCFCCN_CFE_SHIFT 0
#define RSCANFD_CFDCFCCN_CFE_DISABLE 0
#define RSCANFD_CFDCFCCN_CFE_ENABLE 1

/* Common FIFO Configuration / Control Enhancement Register N */
#define RSCANFD_CFDCFCCEN 0x0180
/* Common FIFO Configuration / Control Enhancement Register N bits */
#define RSCANFD_CFDCFCCEN_CFBME_SHIFT 16
#define RSCANFD_CFDCFCCEN_CFBME_ENABLE 0

/* Common FIFO Status Registers N */
#define RSCANFD_CFDCFSTSN 0x01E0
/* Common FIFO Status Registers N bits */
#define RSCANFD_CFDCFSTSN_CFTXIF BIT(4)
#define RSCANFD_CFDCFSTSN_CFRXIF BIT(3)
#define RSCANFD_CFDCFSTSN_CFEMP BIT(0)

/* Common FIFO Pointer Control Register 0 */
#define RSCANFD_CFDCFPCTR0 0x0240
/* Common FIFO Pointer Control Register 1 */
#define RSCANFD_CFDCFPCTR1 0x0244

/* Global Acceptance Filter List ID Register N */
#define RSCANFD_CFDGAFLIDN 0x1800

/* Global Acceptance Filter List Mask Register N */
#define RSCANFD_CFDGAFLMN 0x1804

/* Global Acceptance Filter List Pointer 0 Register N */
#define RSCANFD_CFDGAFLP0N 0x1808

/* Global Acceptance Filter List Pointer 1 Register N */
#define RSCANFD_CFDGAFLP1N 0x180C

/* Common FIFO Access ID Registers B N */
#define RSCANFD_CFDCFIDBN 0x6400
/* Common FIFO Access ID Registers B N bits */
#define RSCANFD_CFDCFIDBN_CFIDE BIT(31)
#define RSCANFD_CFDCFIDBN_CFRTR BIT(30)
#define RSCANFD_CFDCFIDBN_CFID_MASK 0x1FFFFFFF
#define RSCANFD_CFDCFIDBN_CFID_SHIFT 0

/* Common FIFO Access Pointer Registers B N */
#define RSCANFD_CFDCFPTRBN 0x6404
/* Common FIFO Access Pointer Registers B N bits */
#define RSCANFD_CFDCFPTRBN_CFDLC_MASK 0x0F
#define RSCANFD_CFDCFPTRBN_CFDLC_SHIFT 28
#define RSCANFD_CFDCFPTRBN_CFTS_MASK 0xFFFF
#define RSCANFD_CFDCFPTRBN_CFTS_SHIFT 0

/* Common FIFO Access CAN-FD Control/Status Register B N */
#define RSCANFD_CFDCFFDCSTSBN 0x6408
/* Common FIFO Access CAN-FD Control/Status Register B N bits */
#define RSCANFD_CFDCFFDCSTSBN_CFFDF BIT(2)
#define RSCANFD_CFDCFFDCSTSBN_CFBRS BIT(1)
#define RSCANFD_CFDCFFDCSTSBN_CFESI BIT(0)

/* Common FIFO Access Data Field 0 Registers B N */
#define RSCANFD_CFDCFDF0BN 0x640C

#define CAN_RCAR_RSCANFD_MODULE_CLOCK_RATE 80000000

/** The amount of channels per controller. */
#define CAN_RCAR_RSCANFD_CHANNELS_COUNT 8

/** How many Common FIFO are assigned to each channel. */
#define CAN_RCAR_RSCANFD_COMMON_FIFO_PER_CHANNEL 3

/* There are four consecutive 4-byte registers per entry */
#define CAN_RCAR_RSCANFD_AFL_ENTRY_SIZE 16

/* There are 32 consecutive 4-byte registers per entry (only the first 19 are used). */
#define CAN_RCAR_RSCANFD_CFMBCP_ENTRY_SIZE 128

/* TODO */
#define CAN_RCAR_RSCANFD_CHANNEL_REGISTERS_GROUP_SIZE 16

struct can_rcar_rscanfd_global_config {
	uint32_t reg;
	const struct device *clock_dev;
	// TODO utiliser API clock générique
	clock_control_subsys_t global_clk;
	clock_control_subsys_t module_clk;
	uint32_t num_enabled_channels;
};

struct can_rcar_rscanfd_global_data {
	struct k_mutex list_mutex;
	const struct device *channel_dev[CAN_RCAR_RSCANFD_CHANNELS_COUNT];
};

struct can_rcar_rscanfd_config {
	const struct can_driver_config common;
	const struct device *global_dev;
	uint32_t reg;
	uint32_t channel;
	//int reg_size;
	//init_func_t init_func;
	//const struct device *clock_dev;
/*#ifdef CONFIG_SOC_SERIES_RCAR_GEN5 // TODO
	clock_control_subsys_t mod_clk;
	clock_control_subsys_t bus_clk;
#else
	struct rcar_cpg_clk mod_clk;
	struct rcar_cpg_clk bus_clk;
#endif*/
	// TODO utiliser API clock générique
	//clock_control_subsys_t clk;
	const struct pinctrl_dev_config *pcfg;
	void (*configure_irq)(void);
};

struct can_rcar_rscanfd_data {
	struct can_driver_data common;
	struct k_mutex inst_mutex;
	can_tx_callback_t tx_callback;
	void *tx_user_data;
	struct k_sem tx_sem;
	can_rx_callback_t rx_callback[CONFIG_CAN_RCAR_MAX_FILTERS];
	void *rx_callback_user_data[CONFIG_CAN_RCAR_MAX_FILTERS];
	struct can_filter filter[CONFIG_CAN_RCAR_MAX_FILTERS];
	struct k_mutex rx_mutex;
};

struct can_rcar_rscanfd_channel_device_node {
	sys_snode_t node;
	const struct device *dev;
};

/*
 * Read a 32-bit value from the device registers map.
 * @param dev TODO
 */
static inline uint32_t can_rcar_rscanfd_read(const struct device *dev, uint32_t offset)
{
	const struct can_rcar_rscanfd_config *config = dev->config;

	// TODO MMIO
	return sys_read32(config->reg + offset);
}

static inline void can_rcar_rscanfd_write(const struct device *dev,
					      uint32_t offset, uint32_t value)
{
	const struct can_rcar_rscanfd_config *config = dev->config;

	// TODO MMIO
	sys_write32(value, config->reg + offset);
}

/**
 * TODO
 *
 * @note This function can sleep, do not use it in interrupt context.
 **/
static inline int can_rcar_rscanfd_busy_wait(mem_addr_t reg, uint32_t bit_mask, bool expect_one)
{
	const int MAX_RETRIES = 256; // TODO
	int i;
	uint32_t val;

	for (i = 0; i < MAX_RETRIES; i++) {
		/*val = sys_read32(reg);
		if (expect_one) {
			if (reg & bit_mask) {
				return 0;
			}
		}
		else {
			if (!(reg & bit_mask)) {
				return 0;
			}
		}*/
		val = sys_read32(reg) & bit_mask;
		if (!expect_one) val = !val;
		if (val) {
			return 0;
		}

		k_sleep(K_USEC(10));
	}

	LOG_DBG("Busy wait timed out. reg=0x%08lX, bit_mask=%08X, expect_one=%d", reg, bit_mask, expect_one);
	return -EAGAIN;
}

/**
 * TODO
 * @note The reset state is applied at controller level, impacting all channels.
 */
static int can_rcar_rscanfd_enter_reset_mode(const struct can_rcar_rscanfd_global_config *config/*, bool force*/)
{
	/* Request the global reset mode, resetting all other fields in the same time */
	sys_write32(RSCANFD_CFDGCTR_GMDC_GLOBAL_RESET_MODE_REQUEST, config->reg + RSCANFD_CFDGCTR);

	return can_rcar_rscanfd_busy_wait(config->reg + RSCANFD_CFDGSTS, RSCANFD_CFDGSTS_GRSTSTS, 1);
}

/**
 * TODO
 * @note The halt state is applied at the channel level, not impacting the other channels.
 */
static int can_rcar_rscanfd_channel_enter_halt_mode(const struct device *dev)
{
	const struct can_rcar_rscanfd_config *config = dev->config;
	uint32_t base_offset, val;

	base_offset = config->channel * CAN_RCAR_RSCANFD_CHANNEL_REGISTERS_GROUP_SIZE;

	val = can_rcar_rscanfd_read(dev, base_offset + RSCANFD_CFDCNCTR);
	val &= ~(RSCANFD_CFDCNCTR_CHMDC_MASK << RSCANFD_CFDCNCTR_CHMDC_SHIFT);
	val |= RSCANFD_CFDCNCTR_CHMDC_CHANNEL_HALT_REQUEST << RSCANFD_CFDCNCTR_CHMDC_SHIFT;
	can_rcar_rscanfd_write(dev, base_offset + RSCANFD_CFDCNCTR, val);

	return can_rcar_rscanfd_busy_wait(config->reg + base_offset + RSCANFD_CFDCNSTS, RSCANFD_CFDCNSTS_CHLTSTS, 1);
}

/**
 * TODO
 * @note The operation state is applied at controller level, impacting all channels.
 */
static int can_rcar_rscanfd_enter_operation_mode(const struct can_rcar_rscanfd_global_config *config)
{
	uint32_t val;

	val = sys_read32(config->reg + RSCANFD_CFDGCTR);
	val &= ~(RSCANFD_CFDGCTR_GMDC_MASK << RSCANFD_CFDGCTR_GMDC_SHIFT);
	val |= RSCANFD_CFDGCTR_GMDC_GLOBAL_OPERATION_MODE_REQUEST << RSCANFD_CFDGCTR_GMDC_SHIFT;
	sys_write32(val, config->reg + RSCANFD_CFDGCTR);

	return can_rcar_rscanfd_busy_wait(config->reg + RSCANFD_CFDGSTS, RSCANFD_CFDGSTS_GRSTSTS, 0);
}

static int can_rcar_rscanfd_channel_leave_sleep_mode(const struct can_rcar_rscanfd_config *config)
{
	uint32_t base_addr = config->reg + (config->channel * CAN_RCAR_RSCANFD_CHANNEL_REGISTERS_GROUP_SIZE);
	int ret;

	/* Release the channel from sleep mode, resetting all other fields in the same time. */
	//printk("[%s:%d] entry RSCANFD_CFDCNCTR avant = 0x%08X\n", __func__, __LINE__, sys_read32(base_addr + RSCANFD_CFDCNCTR));
	//printk("[%s:%d] entry RSCANFD_CFDCNSTS avant = 0x%08X\n", __func__, __LINE__, sys_read32(base_addr + RSCANFD_CFDCNSTS));
	sys_write32(RSCANFD_CFDCNCTR_CHMDC_CHANNEL_RESET_REQUEST << RSCANFD_CFDCNCTR_CHMDC_SHIFT,
		    base_addr + RSCANFD_CFDCNCTR);

	ret = can_rcar_rscanfd_busy_wait(base_addr + RSCANFD_CFDCNSTS, RSCANFD_CFDCNSTS_CSLPSTS, 0);
	if (ret != 0) {
		LOG_ERR("Leaving the sleep mode for channel %u took too long.", config->channel);
		return ret;
	}

	return 0;
}

static int can_rcar_rscanfd_channel_enter_operation_mode(const struct can_rcar_rscanfd_config *config)
{
	uint32_t base_addr = config->reg + (config->channel * CAN_RCAR_RSCANFD_CHANNEL_REGISTERS_GROUP_SIZE), val;
	int ret;

	/* Determine the current channel status */
	/*val = sys_read32(base_addr + RSCANFD_CFDCNSTS);
	if (val & RSCANFD_CFDCNSTS_CRSTSTS) {
		status_bit = RSCANFD_CFDCNSTS_CRSTSTS*/

	val = sys_read32(base_addr + RSCANFD_CFDCNCTR);
	val &= ~(RSCANFD_CFDCNCTR_CHMDC_MASK << RSCANFD_CFDCNCTR_CHMDC_SHIFT);
	val |= RSCANFD_CFDCNCTR_CHMDC_CHANNEL_OPERATION_REQUEST << RSCANFD_CFDCNCTR_CHMDC_SHIFT;
	sys_write32(val, base_addr + RSCANFD_CFDCNCTR);

	ret = can_rcar_rscanfd_busy_wait(base_addr + RSCANFD_CFDCNSTS, RSCANFD_CFDCNSTS_CRSTSTS, 0);
	if (ret != 0) {
		LOG_ERR("Going from reset to operation mode for channel %u took too long.",
			config->channel);
		return ret;
	}
	ret = can_rcar_rscanfd_busy_wait(base_addr + RSCANFD_CFDCNSTS, RSCANFD_CFDCNSTS_CHLTSTS, 0);
	if (ret != 0) {
		LOG_ERR("Going from halt to operation mode for channel %u took too long.",
			config->channel);
		return ret;
	}
	// TODO sleep ?

	/* Wait for the controller to apply the new state */
	/*for (int i = 0; i < MAX_STR_READS; i++) {
		if (!(sys_read32(config->reg_addr + RSCANFD_CFDCNSTS) & RSCANFD_CFDCNSTS_CSLPSTS)) {
			printk("[%s:%d] RSCANFD_CFDCNCTR OK\n", __func__, __LINE__);
			return 0;
		}
	}*/
	//printk("[%s:%d] entry RSCANFD_CFDCNSTS apres = 0x%08X\n", __func__, __LINE__, sys_read32(base_addr + RSCANFD_CFDCNSTS));

	return 0;
}

/**
 * TODO
 * Configure the rules table by creating one rule that matches them all (reception frames).
 * The reception filters are currently implemented in software.
 */
static void can_rcar_rscanfd_configure_acceptance_filter_list(const struct can_rcar_rscanfd_config *config)
{
	uint32_t base_addr, val, shift;

	/*
	 * Enable write access for the page 0 (set the page index 0 value in the same time).
	 * As all channels will use a single rule, only the first page is needed.
	 */
	sys_write32(RSCANFD_CFDGAFLECTR_AFLDAE, config->reg + RSCANFD_CFDGAFLECTR);

	/* There are two consecutive channel rules per CFDGAFLCFGn register */
	base_addr = config->reg + RSCANFD_CFDGAFLCFGN + (config->channel / 2);
	/* Address the correct channel within the register */
	val = sys_read32(base_addr);
	printk("[%s:%d] RSCANFD_CFDGAFLCFG%d avant = 0x%08X\n", __func__, __LINE__, config->channel, sys_read32(base_addr));
	if (config->channel & 1) { /* Odd channel */
		shift = RSCANFD_CFDGAFLCFGN_RNC_CHANNEL_ODD_SHIFT;
	}
	else {
		shift = RSCANFD_CFDGAFLCFGN_RNC_CHANNEL_EVEN_SHIFT;
	}
	val &= ~(RSCANFD_CFDGAFLCFGN_RNC_CHANNEL_MASK << shift);
	/* Configure one rule for the channel */
	val |= 1 << shift;
	sys_write32(val, base_addr);
	printk("[%s:%d] RSCANFD_CFDGAFLCFG%d apres = 0x%08X\n", __func__, __LINE__, config->channel, sys_read32(base_addr));

	/* A page contains 16 consecutive entries */
	base_addr = config->reg + (config->channel * CAN_RCAR_RSCANFD_AFL_ENTRY_SIZE);
	/* Clear the CAN ID as it won't be taken into account by the mask register */
	sys_write32(0, base_addr + RSCANFD_CFDGAFLIDN);
	/* Accept all received CAN frames */
	sys_write32(0, base_addr + RSCANFD_CFDGAFLMN);
	/* Disable the DLC check */
	sys_write32(0, base_addr + RSCANFD_CFDGAFLP0N); // TODO GAFLSRD1
	/* Use the second Common FIFO dedicated to each channel as target for reception */
	val = config->channel * CAN_RCAR_RSCANFD_COMMON_FIFO_PER_CHANNEL + 1;
	sys_write32(0x100 << val, base_addr + RSCANFD_CFDGAFLP1N);

	/* Disable write access for page 0 */
	sys_write32(0, config->reg + RSCANFD_CFDGAFLECTR);
}

static void can_rcar_rscanfd_configure_fifo(const struct can_rcar_rscanfd_config *config)
{
	uint32_t base_addr;

	/* Use the first Common FIFO from the several available for each channel. */
	base_addr = config->reg +
		(config->channel * sizeof(uint32_t) * CAN_RCAR_RSCANFD_COMMON_FIFO_PER_CHANNEL);

	/* Dedicate the first Common FIFO to transmission */
	sys_write32(RSCANFD_CFDCFCCN_CFDC_DEPTH_64 << RSCANFD_CFDCFCCN_CFDC_SHIFT |
		RSCANFD_CFDCFCCN_CFIM_EVERY_MESSAGE << RSCANFD_CFDCFCCN_CFIM_SHIFT |
		RSCANFD_CFDCFCCN_CFM_TX << RSCANFD_CFDCFCCN_CFM_SHIFT |
		RSCANFD_CFDCFCCN_CFPLS_SIZE_64 << RSCANFD_CFDCFCCN_CFPLS_SHIFT |
		RSCANFD_CFDCFCCN_CFTXIE_INT_ENABLED << RSCANFD_CFDCFCCN_CFTXIE_SHIFT |
		RSCANFD_CFDCFCCN_CFE_DISABLE << RSCANFD_CFDCFCCN_CFE_SHIFT,
		base_addr + RSCANFD_CFDCFCCN); // TODO transmission delay dans param DT ? int

	/* Allow transmitting from the Common FIFO */
	sys_write32(RSCANFD_CFDCFCCEN_CFBME_ENABLE << RSCANFD_CFDCFCCEN_CFBME_SHIFT,
		base_addr + RSCANFD_CFDCFCCEN);

	/* Use the second Common FIFO for reception */
	base_addr += sizeof(uint32_t);
	sys_write32(RSCANFD_CFDCFCCN_CFDC_DEPTH_64 << RSCANFD_CFDCFCCN_CFDC_SHIFT |
		RSCANFD_CFDCFCCN_CFIM_EVERY_MESSAGE << RSCANFD_CFDCFCCN_CFIM_SHIFT |
		RSCANFD_CFDCFCCN_CFM_RX << RSCANFD_CFDCFCCN_CFM_SHIFT |
		RSCANFD_CFDCFCCN_CFPLS_SIZE_64 << RSCANFD_CFDCFCCN_CFPLS_SHIFT |
		RSCANFD_CFDCFCCN_CFRXIE_INT_ENABLED << RSCANFD_CFDCFCCN_CFRXIE_SHIFT |
		RSCANFD_CFDCFCCN_CFE_DISABLE << RSCANFD_CFDCFCCN_CFE_SHIFT,
		base_addr + RSCANFD_CFDCFCCN);
}

/**
 * TODO
 *
 * @note The channel must already be in Reset or Halt state.
 */
static int can_rcar_rscanfd_configure_timing(const struct can_rcar_rscanfd_config *config,
					     const struct can_timing *timing)
{
	uint32_t base_addr;

	LOG_DBG("Set timing for channel %u, sjw=%u, prop_seg=%u, seg1=%u, seg2=%u, presc=%u.",
		config->channel, timing->sjw, timing->prop_seg, timing->phase_seg1, timing->phase_seg2,
		timing->prescaler);

	/* Each channel is handled by a group of four 4-byte registers */
	base_addr = config->reg + (config->channel * CAN_RCAR_RSCANFD_CHANNEL_REGISTERS_GROUP_SIZE);

	// TODO prop seg
	if ((timing->sjw < 1) || (timing->sjw > 128)) {
		LOG_DBG("Invalid synchronisation jump width setting %u.\n", timing->sjw);
		return -EINVAL;
	}
	//if ((timing->prop_seg
	if ((timing->phase_seg1 < 2) || (timing->phase_seg1 > 256)) {
		LOG_DBG("Invalid phase segment 1 setting %u.\n", timing->phase_seg1);
		return -EINVAL;
	}
	if ((timing->phase_seg2 < 2) || (timing->phase_seg2 > 128)) {
		LOG_DBG("Invalid phase segment 2 setting %u.\n", timing->phase_seg1);
		return -EINVAL;
	}
	if (timing->prescaler > 1024) {
		LOG_DBG("Invalid prescaler setting %u.\n", timing->prescaler);
		return -EINVAL;
	}
	sys_write32((timing->phase_seg1 - 1) << RSCANFD_CFDCNNCFG_NTSEG1_SHIFT |
		(timing->phase_seg2 - 1) << RSCANFD_CFDCNNCFG_NTSEG2_SHIFT |
		timing->sjw << RSCANFD_CFDCNNCFG_NSJW_SHIFT |
		(timing->prescaler - 1) << RSCANFD_CFDCNNCFG_NBRP_SHIFT,
		base_addr + RSCANFD_CFDCNNCFG);

	// TODO CFDCnCTR

	//printk("[%s:%d] CFDCnNCFG %u = 0x%08X\n", __func__, __LINE__, config->channel, sys_read32(base_addr + RSCANFD_CFDCNNCFG));

	return 0;
}

static int can_rcar_rscanfd_get_capabilities(const struct device *dev, can_mode_t *cap)
{
	ARG_UNUSED(dev);

	printk("[%s:%d] entry\n", __func__, __LINE__);
	// TODO
	*cap = CAN_MODE_NORMAL | CAN_MODE_LOOPBACK | CAN_MODE_LISTENONLY | CAN_MODE_FD;

	return 0;
}

static int can_rcar_rscanfd_start(const struct device *dev)
{
	const struct can_rcar_rscanfd_config *config = dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	uint32_t base_addr, val;
	int ret;

	printk("[%s:%d] entry canal %u\n", __func__, __LINE__, config->channel);

	if (data->common.started) {
		return -EALREADY;
	}

	if (config->common.phy != NULL) {
		ret = can_transceiver_enable(config->common.phy, data->common.mode);
		if (ret != 0) {
			LOG_ERR("Failed to enable the CAN transceiver for canal %u (%d).",
				config->channel, ret);
			return ret;
		}
	}

	k_mutex_lock(&data->inst_mutex, K_FOREVER);

	CAN_STATS_RESET(dev);

	printk("[%s:%d] reg av op 0x%08X\n", __func__, __LINE__, sys_read32(config->reg + 4));

	ret = can_rcar_rscanfd_channel_enter_operation_mode(config);
	if (ret != 0) {
		goto exit_unlock;
	}

	printk("[%s:%d] reg ap op 0x%08X\n", __func__, __LINE__, sys_read32(config->reg + 4));

	/* The FIFOs can be enabled only when the channel is in operation mode */
	/* Transmission FIFO, the first Common FIFO dedicated to the channel */
	base_addr = config->reg +
		(config->channel * sizeof(uint32_t) * CAN_RCAR_RSCANFD_COMMON_FIFO_PER_CHANNEL);
	val = sys_read32(base_addr + RSCANFD_CFDCFCCN);
	val &= ~(RSCANFD_CFDCFCCN_CFE_MASK << RSCANFD_CFDCFCCN_CFE_SHIFT);
	val |= RSCANFD_CFDCFCCN_CFE_ENABLE << RSCANFD_CFDCFCCN_CFE_SHIFT;
	sys_write32(val, base_addr + RSCANFD_CFDCFCCN);

	/* Reception FIFO, the second Common FIFO dedicated to the channel */
	base_addr += sizeof(uint32_t);
	val = sys_read32(base_addr + RSCANFD_CFDCFCCN);
	val &= ~(RSCANFD_CFDCFCCN_CFE_MASK << RSCANFD_CFDCFCCN_CFE_SHIFT);
	val |= RSCANFD_CFDCFCCN_CFE_ENABLE << RSCANFD_CFDCFCCN_CFE_SHIFT;
	sys_write32(val, base_addr + RSCANFD_CFDCFCCN);

	data->common.started = true;

	// TEST
	printk("[%s:%d] CFDCNNCFG0=0x%08X, CFDCNNCFG1=0x%08X, CFDCNCTR0=0x%08X, CFDCNCTR1=0x%08X, CFDCNSTS0=0x%08X, CFDCNSTS1=0x%08X, CFDCFCCN0=0x%08X, CFDCFCCN1=0x%08X, CFDCFCCEN0=0x%08X, CFDCFCCEN1=0x%08X, CFDCFSTS0=0x%08X, CFDCFSTS1=0x%08X, CFDRFCCN0=0x%08X, CFDRFCCN1=0x%08X, CFDGAFLP1N0=0x%08X, CFDGAFLP1N1=0x%08X\n", __func__, __LINE__, sys_read32(config->reg), sys_read32(config->reg + 16), sys_read32(config->reg + 4), sys_read32(config->reg + 4 + 16), sys_read32(config->reg + 8), sys_read32(config->reg + 8 + 16), sys_read32(config->reg + 0x120), sys_read32(config->reg + 0x120 + (3*4)), sys_read32(config->reg + 0x180), sys_read32(config->reg + 0x180 + (3*4)), sys_read32(config->reg + 0x1E0), sys_read32(config->reg + 0x1E0 + (3*4)), sys_read32(config->reg + 0xC0), sys_read32(config->reg + 0xC0 + 4), sys_read32(config->reg + 0x180C), sys_read32(config->reg + 0x180C + 16));

	ret = 0;

exit_unlock:
	k_mutex_unlock(&data->inst_mutex);

	return ret;
}

static int can_rcar_rscanfd_stop(const struct device *dev)
{
	const struct can_rcar_rscanfd_config *config = dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	int ret;

	printk("[%s:%d] entry\n", __func__, __LINE__);

	if (!data->common.started) {
		return -EALREADY;
	}

	// TODO
	//k_mutex_lock(&data->inst_mutex, K_FOREVER);

	ret = can_rcar_rscanfd_channel_enter_halt_mode(dev);
	if (ret != 0) {
		return ret;
	}

	if (config->common.phy != NULL) {
		ret = can_transceiver_disable(config->common.phy);
		if (ret != 0) {
			LOG_ERR("Failed to disable CAN transceiver (%d).", ret);
			return ret;
		}
	}



	// TODO
	return 0;
}

static int can_rcar_rscanfd_set_mode(const struct device *dev, can_mode_t mode)
{
	const struct can_rcar_rscanfd_config *config = dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	uint32_t base_offset, val;
	int ret;

	LOG_DBG("Set mode %u for channel %u (current mode is %u).",
		mode, config->channel, data->common.mode);

	if (data->common.started) {
		return -EBUSY;
	}

	/* Safety checks */
	if ((mode & (CAN_MODE_LOOPBACK | CAN_MODE_LISTENONLY)) ==
	    (CAN_MODE_LOOPBACK | CAN_MODE_LISTENONLY)) {
		LOG_ERR("Enabling both loopback and listen-only modes is not supported.");
		return -EIO;
	}
	if (mode & CAN_MODE_ONE_SHOT) {
		LOG_ERR("The one-shot mode is not supported.");
		return -EIO;
	}
	if (mode & CAN_MODE_3_SAMPLES) {
		LOG_ERR("The 3 samples mode is not supported.");
		return -EIO;
	}
#ifndef CONFIG_CAN_FD_MODE
	if (mode & CAN_MODE_FD) {
		LOG_ERR("The flexible data rate mode is not enabled. See CONFIG_CAN_FD_MODE.");
		return -EIO;
	}
#endif

	/* Each channel is handled by a group of four 4-byte registers */
	base_offset = config->channel * CAN_RCAR_RSCANFD_CHANNEL_REGISTERS_GROUP_SIZE;

	k_mutex_lock(&data->inst_mutex, K_FOREVER);

	val = can_rcar_rscanfd_read(dev, base_offset + RSCANFD_CFDCNCTR);
	val &= ~((RSCANFD_CFDCNCTR_CTMS_MASK << RSCANFD_CFDCNCTR_CTMS_SHIFT) |
		(RSCANFD_CFDCNCTR_CTME_MASK << RSCANFD_CFDCNCTR_CTME_SHIFT));

	if (mode & CAN_MODE_LOOPBACK) {
		val |= (RSCANFD_CFDCNCTR_CTMS_INTERNAL_LOOPBACK_MODE << RSCANFD_CFDCNCTR_CTMS_SHIFT) |
			(RSCANFD_CFDCNCTR_CTME_ENABLED << RSCANFD_CFDCNCTR_CTME_SHIFT);
	}
	else if (mode & CAN_MODE_LISTENONLY) {
		val |= (RSCANFD_CFDCNCTR_CTMS_LISTEN_ONLY_MODE << RSCANFD_CFDCNCTR_CTMS_SHIFT) |
			(RSCANFD_CFDCNCTR_CTME_ENABLED << RSCANFD_CFDCNCTR_CTME_SHIFT);
	}
	else if (mode & CAN_MODE_NORMAL) {
		val |= RSCANFD_CFDCNCTR_CTME_DISABLED << RSCANFD_CFDCNCTR_CTME_SHIFT;
	}
	can_rcar_rscanfd_write(dev, base_offset + RSCANFD_CFDCNCTR, val);


#ifdef CONFIG_CAN_FD_MODE
	if (mode & CAN_MODE_FD) {
		// TODO
	}
#endif

	// TODO manual mode

	data->common.mode = mode;

	k_mutex_unlock(&data->inst_mutex);

	return 0;
}

static int can_rcar_rscanfd_set_timing(const struct device *dev, const struct can_timing *timing)
{
	const struct can_rcar_rscanfd_config *config = dev->config;
	int ret;

	// TODO verif init OK ?

	// TODO reset mode, avec

	/*ret = can_rcar_rscanfd_channel_enter_halt_mode(config); // TODO
	if (ret != 0) {
		return -EIO;
	}*/

	// TODO
	return 0;
}

static int can_rcar_rscanfd_send(const struct device *dev, const struct can_frame *frame,
			    k_timeout_t timeout, can_tx_callback_t callback,
			    void *user_data)
{
	const struct can_rcar_rscanfd_config *config = dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	uint32_t base_offset, val, i;

	printk("[%s:%d] entry\n", __func__, __LINE__);

	LOG_DBG("Sending %s, ID: 0x%X, ID type: %s, DLC: %u, remote frame: %s.",
		dev->name, frame->id,
		(frame->flags & CAN_FRAME_IDE) != 0 ? "extended" : "standard",
		frame->dlc,
		(frame->flags & CAN_FRAME_RTR) != 0 ? "yes" : "no");

	if (frame->dlc > CAN_MAX_DLC) {
		LOG_ERR("DLC of %d exceeds maximum (%d).", frame->dlc, CAN_MAX_DLC);
		return -EINVAL;
	}

	if ((frame->flags & ~(CAN_FRAME_IDE | CAN_FRAME_RTR)) != 0) {
		// TODO
		LOG_ERR("Unsupported CAN frame flags 0x%02X.", frame->flags);
		return -ENOTSUP;
	}

	if (!data->common.started) {
		return -ENETDOWN;
	}

	// TODO timestamp

	if (k_sem_take(&data->tx_sem, timeout) != 0) {
		return -EAGAIN;
	}

	k_mutex_lock(&data->inst_mutex, K_FOREVER);

	data->tx_callback = callback;
	data->tx_user_data = user_data;

	/* The first Common FIFO is used for transmission */
	base_offset = config->channel * CAN_RCAR_RSCANFD_COMMON_FIFO_PER_CHANNEL *
		CAN_RCAR_RSCANFD_CFMBCP_ENTRY_SIZE;

	/* ID and flags */
	val = (frame->id & RSCANFD_CFDCFIDBN_CFID_MASK) << RSCANFD_CFDCFIDBN_CFID_SHIFT;
	if (frame->flags & CAN_FRAME_IDE) {
		val |= RSCANFD_CFDCFIDBN_CFIDE;
	}
	if (frame->flags & CAN_FRAME_RTR) {
		val |= RSCANFD_CFDCFIDBN_CFRTR;
	}
	can_rcar_rscanfd_write(dev, base_offset + RSCANFD_CFDCFIDBN, val);

	/* DLC and timestamp TODO */
	can_rcar_rscanfd_write(dev, base_offset + RSCANFD_CFDCFPTRBN,
		(frame->dlc & RSCANFD_CFDCFPTRBN_CFDLC_MASK) << RSCANFD_CFDCFPTRBN_CFDLC_SHIFT);

	/* CAN-FD settings */
	val = 0;
	if (frame->flags & CAN_FRAME_FDF) {
		val |= RSCANFD_CFDCFFDCSTSBN_CFFDF;
	}
	if (frame->flags & CAN_FRAME_BRS) {
		val |= RSCANFD_CFDCFFDCSTSBN_CFBRS;
	}
	if (frame->flags & CAN_FRAME_ESI) {
		val |= RSCANFD_CFDCFFDCSTSBN_CFESI;
	}
	can_rcar_rscanfd_write(dev, base_offset + RSCANFD_CFDCFFDCSTSBN, val);

	/* Data */
	val = can_dlc_to_bytes(frame->dlc);
	for (i = 0; i < val; i++) {
		sys_write8(frame->data[i], config->reg + base_offset + RSCANFD_CFDCFDF0BN + i);
	}

	/* Ask for message transmission */
	base_offset = config->channel * CAN_RCAR_RSCANFD_COMMON_FIFO_PER_CHANNEL * sizeof(uint32_t);
	can_rcar_rscanfd_write(dev, base_offset + RSCANFD_CFDCFPCTR0, 0xFF);

	k_mutex_unlock(&data->inst_mutex);

	return 0;
}

static int can_rcar_rscanfd_add_rx_filter(const struct device *dev, can_rx_callback_t callback,
					  void *user_data, const struct can_filter *filter)
{
	struct can_rcar_rscanfd_data *data = dev->data;
	int i, ret;

	printk("[%s:%d] entry\n", __func__, __LINE__);

	k_mutex_lock(&data->rx_mutex, K_FOREVER);

	/* Search for the first empty entry */
	for (i = 0; i < CONFIG_CAN_RCAR_MAX_FILTERS; i++) {
		if (data->rx_callback[i] == NULL) {
			data->rx_callback_user_data[i] = user_data;
			data->filter[i] = *filter;
			data->rx_callback[i] = callback;
			break;
		}
	}

	k_mutex_unlock(&data->rx_mutex);

	if (i == CONFIG_CAN_RCAR_MAX_FILTERS) {
		ret = -ENOSPC;
	}
	else {
		ret = i;
	}

	return ret;
}

static void can_rcar_rscanfd_remove_rx_filter(const struct device *dev, int filter_id)
{
	struct can_rcar_rscanfd_data *data = dev->data;

	printk("[%s:%d] entry\n", __func__, __LINE__);

	if ((filter_id < 0) || (filter_id >= CONFIG_CAN_RCAR_MAX_FILTERS)) {
		LOG_ERR("Filter ID %d is out of bounds.", filter_id);
		return;
	}

	k_mutex_lock(&data->rx_mutex, K_FOREVER);
	data->rx_callback[filter_id] = NULL;
	k_mutex_unlock(&data->rx_mutex);
}

static int can_rscanfd_get_state(const struct device *dev, enum can_state *state,
			      struct can_bus_err_cnt *err_cnt)
{
	printk("[%s:%d] entry\n", __func__, __LINE__);
	// TODO
	return 0;
}

static int can_rcar_rscanfd_get_core_clock(const struct device *dev, uint32_t *rate)
{
	const struct can_rcar_rscanfd_config *config = dev->config;
	const struct can_rcar_rscanfd_global_config *global_config = config->global_dev->config;

	return clock_control_get_rate(global_config->clock_dev, global_config->global_clk, rate);
}

static int can_rcar_rscanfd_get_max_filters(const struct device *dev, bool ide)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(ide);

	return CONFIG_CAN_RCAR_MAX_FILTERS;
}

static void can_rcar_rscanfd_rx_isr(const struct device *dev)
{
	const struct can_rcar_rscanfd_config *config = dev->config;
	const struct can_rcar_rscanfd_data *data = dev->data;
	uint32_t val, i, bytes_count, base_offset, source_addr;
	struct can_frame frame, user_frame;
	uint8_t *can_data;

	/* The channel second Common FIFO is used for reception */
	base_offset = config->channel * CAN_RCAR_RSCANFD_COMMON_FIFO_PER_CHANNEL *
		CAN_RCAR_RSCANFD_CFMBCP_ENTRY_SIZE + CAN_RCAR_RSCANFD_CFMBCP_ENTRY_SIZE;

	frame.flags = 0;
	val = can_rcar_rscanfd_read(dev, base_offset + RSCANFD_CFDCFIDBN);
	if (val & RSCANFD_CFDCFIDBN_CFIDE) {
		frame.flags |= CAN_FRAME_IDE;
	}
	if (val & RSCANFD_CFDCFIDBN_CFRTR) {
#ifndef CONFIG_CAN_ACCEPT_RTR
		goto exit_unlock;
#endif
		frame.flags |= CAN_FRAME_RTR;
	}
	frame.id = (val >> RSCANFD_CFDCFIDBN_CFID_SHIFT) & RSCANFD_CFDCFIDBN_CFID_MASK;

	val = can_rcar_rscanfd_read(dev, base_offset + RSCANFD_CFDCFPTRBN);
	frame.dlc = (val >> RSCANFD_CFDCFPTRBN_CFDLC_SHIFT) & RSCANFD_CFDCFPTRBN_CFDLC_MASK;
#ifdef CONFIG_CAN_RX_TIMESTAMP
	frame.timestamp = (val >> RSCANFD_CFDCFPTRBN_CFTS_SHIFT) & RSCANFD_CFDCFPTRBN_CFTS_MASK;
#endif

	val = can_rcar_rscanfd_read(dev, base_offset + RSCANFD_CFDCFFDCSTSBN);
	if (val & RSCANFD_CFDCFFDCSTSBN_CFESI) {
		frame.flags |= CAN_FRAME_ESI;
	}
	if (val & RSCANFD_CFDCFFDCSTSBN_CFBRS) {
		// TODO BRS only in FD mode ?
		frame.flags |= CAN_FRAME_BRS;
	}
	if (val & RSCANFD_CFDCFFDCSTSBN_CFFDF) {
#ifndef CONFIG_CAN_FD_MODE
		goto exit_unlock;
#endif
		frame.flags |= CAN_FRAME_FDF;
	}

	/* A CAN 2.0 frame data size is limited to 8 bytes */
	bytes_count = can_dlc_to_bytes(frame.dlc);
	if (!(frame.flags & CAN_FRAME_FDF) && (bytes_count > CAN_MAX_DLC)) {
		goto exit_unlock;
	}

	//printk("[%s:%d] ID=%u, DLC=%u, bytes_count=%u\n", __func__, __LINE__, frame.id, frame.dlc, bytes_count);

	/* Retrieve the data */
	source_addr = config->reg + base_offset + RSCANFD_CFDCFDF0BN;
	can_data = frame.data;
	for (i = 0; i < bytes_count; i++) {
		*can_data = sys_read8(source_addr);
		source_addr++;
		can_data++;
		//printk("[%s:%d] data %d=0x%02X\n", __func__, __LINE__, i, *(can_data - 1));
	}

	/* Check for all matching filters */
	for (i = 0; i < CONFIG_CAN_RCAR_MAX_FILTERS; i++) {
		if (data->rx_callback[i] == NULL) {
			continue;
		}

		if (!can_frame_matches_filter(&frame, &data->filter[i])) {
			continue;
		}

		/*
		 * Make a temporary copy in case the user modifies the message in a first matching
		 * filter callback, and another filter also matches afterwards.
		 */
		user_frame = frame;
		data->rx_callback[i](dev, &user_frame, data->rx_callback_user_data[i]);
	}

exit_unlock:
	/* Increment the FIFO read pointer to get access to the next received frame */
	base_offset = config->channel * CAN_RCAR_RSCANFD_COMMON_FIFO_PER_CHANNEL * sizeof(uint32_t);
	can_rcar_rscanfd_write(dev, base_offset + RSCANFD_CFDCFPCTR1, 0xFF);
}

static void can_rcar_rscanfd_isr(const struct device *dev)
{
	const struct can_rcar_rscanfd_config *config = dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	uint32_t irq_flags, base_offset;

	printk("[%s:%d] entry dev=%s\n", __func__, __LINE__, dev->name);

	/* Frame transmission, uses the first Common FIFO of the channel */
	base_offset = config->channel * sizeof(uint32_t) * CAN_RCAR_RSCANFD_COMMON_FIFO_PER_CHANNEL;
	irq_flags = can_rcar_rscanfd_read(dev, base_offset + RSCANFD_CFDCFSTSN);
	if (irq_flags & RSCANFD_CFDCFSTSN_CFTXIF) {
		data->tx_callback(dev, 0 /* TODO */, data->tx_user_data);
		k_sem_give(&data->tx_sem);

		/* Clear the interrupt flag */
		irq_flags &= ~RSCANFD_CFDCFSTSN_CFTXIF;
		can_rcar_rscanfd_write(dev, base_offset + RSCANFD_CFDCFSTSN, irq_flags);
	}

	/* Frame reception, uses the second Common FIFO of the channel */
	base_offset += sizeof(uint32_t);
	irq_flags = can_rcar_rscanfd_read(dev, base_offset + RSCANFD_CFDCFSTSN);
	if (irq_flags & RSCANFD_CFDCFSTSN_CFRXIF) {
		/* Handle all the messages contained in the FIFO */
		do {
			can_rcar_rscanfd_rx_isr(dev);
		} while (!(can_rcar_rscanfd_read(dev, base_offset + RSCANFD_CFDCFSTSN)
			& RSCANFD_CFDCFSTSN_CFEMP));

		/* Clear the interrupt flag */
		irq_flags &= ~RSCANFD_CFDCFSTSN_CFRXIF;
		can_rcar_rscanfd_write(dev, base_offset + RSCANFD_CFDCFSTSN, irq_flags);
	}
}

/*static int can_rcar_rscanfd_channel_dev_visitor_callback(const struct device *dev, void *context)
{
	printk("[%s:%d] VISITOR %s\n", dev->name);
	return 0;
}*/

static int can_rcar_rscanfd_init(const struct device *dev)
{
	static uint32_t enabled_channels_count = 0;
	const struct can_rcar_rscanfd_config *config = dev->config;
	const struct can_rcar_rscanfd_global_config *global_config = config->global_dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	struct can_rcar_rscanfd_global_data *global_data = config->global_dev->data;
	struct can_timing timing /*= {0}*/;
	int ret, i;

	printk("[%s:%d] entry DRIVER CUSTOM CHANNEL %d\n", __func__, __LINE__, config->channel);

	ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		LOG_ERR("Failed to apply the pinctrl default state.");
		return ret;
	}

	ret = can_rcar_rscanfd_channel_leave_sleep_mode(config);
	if (ret != 0) {
		return ret;
	}

	can_rcar_rscanfd_configure_acceptance_filter_list(config);

	can_rcar_rscanfd_configure_fifo(config);

	// TEST
	/*if (config->channel == 0) {
		printk("[%s:%d] init channel 0 seulement\n", __func__, __LINE__);*/

	ret = can_calc_timing(dev, &timing, config->common.bitrate, config->common.sample_point);
	if (ret < 0) {
		LOG_ERR("Failed to find a timing for channel %u bit rate %u bit/s (%d).",
			config->channel, config->common.bitrate, ret);
		return ret;
	}
	//LOG_DBG("Prescaler: %d, TS1: %d, TS2: %d, Sample-point error: %d.",
	//	timing.prescaler, timing.phase_seg1, timing.phase_seg2, ret); // REDONDANT

	ret = can_rcar_rscanfd_configure_timing(config, &timing);
	if (ret != 0) {
		return ret;
	}

	// TEST
	//}

	config->configure_irq();

	k_mutex_init(&data->inst_mutex);
	k_sem_init(&data->tx_sem, 1, 1);
	k_mutex_init(&data->rx_mutex);

	/* Register the channel device into the controller for later use */
	k_mutex_lock(&global_data->list_mutex, K_FOREVER);
	global_data->channel_dev[enabled_channels_count] = dev;
	k_mutex_unlock(&global_data->list_mutex);

	/* Switch the controller to operation mode when all channels are initialized */
	enabled_channels_count++;
	if (enabled_channels_count == global_config->num_enabled_channels) {
		ret = can_rcar_rscanfd_enter_operation_mode(global_config);
		if (ret) {
			LOG_ERR("Failed to put the controller in operation mode.");
			// TODO uninit ?
			return ret;
		}

		/*
		 * The channels must go to halt mode while they are not used, also because this
		 * allows some additional channel configuration not possible in reset mode.
		 * The controller needs to be in operation mode.
		 *
		 * Note : no need for mutex here, because only the last registered channel
		 * init function can call this code.
		 */
		for (i = 0; i < global_config->num_enabled_channels; i++) {
			ret = can_rcar_rscanfd_channel_enter_halt_mode(global_data->channel_dev[i]);
			if (ret != 0) {
				LOG_ERR("Failed to switch the CAN device channel \"%s\" to halt mode.",
					global_data->channel_dev[i]->name);
				return ret;
			}

			/* Some working modes can only be set when the channel is in halt mode. */
			ret = can_rcar_rscanfd_set_mode(dev, CAN_MODE_NORMAL);
			if (ret) {
				LOG_ERR("Failed to set the CAN device channel \"%s\" mode to normal.",
					global_data->channel_dev[i]->name);
				return ret;
			}
		}

		LOG_DBG("All channels were successfully initialized.");
	}

	return 0;
}

static DEVICE_API(can, can_rcar_rscanfd_driver_api) = {
	.get_capabilities = can_rcar_rscanfd_get_capabilities,
	.start = can_rcar_rscanfd_start,
	.stop = can_rcar_rscanfd_stop,
	.set_mode = can_rcar_rscanfd_set_mode, // OK
	.set_timing = can_rcar_rscanfd_set_timing,
	.send = can_rcar_rscanfd_send,
	.add_rx_filter = can_rcar_rscanfd_add_rx_filter, // OK
	.remove_rx_filter = can_rcar_rscanfd_remove_rx_filter, // OK
	.get_state = can_rscanfd_get_state,
/*#ifdef CONFIG_CAN_MANUAL_RECOVERY_MODE
	.recover = can_rcar_recover,
#endif*/ /* CONFIG_CAN_MANUAL_RECOVERY_MODE */
	//.set_state_change_callback = can_rcar_set_state_change_callback,
	.get_core_clock = can_rcar_rscanfd_get_core_clock, // OK
	.get_max_filters = can_rcar_rscanfd_get_max_filters, // OK
	// TODO
	.timing_min = {
		.sjw = 0x1,
		.prop_seg = 0x00,
		.phase_seg1 = 0x04,
		.phase_seg2 = 0x02,
		.prescaler = 0x01
	},
	.timing_max = {
		.sjw = 0x4,
		.prop_seg = 0x00,
		.phase_seg1 = 0x10,
		.phase_seg2 = 0x08,
		.prescaler = 0x400
	}
};

/*
 * A CAN controller channel.
 */
#define RSCANFD_INIT(n)							\
	PINCTRL_DT_INST_DEFINE(n);						\
	\
	static void can_rcar_rscanfd_configure_irq_##n(void) \
	{ \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), \
			    can_rcar_rscanfd_isr, DEVICE_DT_INST_GET(n), 0); \
		irq_enable(DT_INST_IRQN(n)); \
	} \
	\
	static const struct can_rcar_rscanfd_config can_rcar_rscanfd_config_##n = {		\
		.common = CAN_DT_DRIVER_CONFIG_INST_GET(n, 0, 1000000),		\
		.global_dev = DEVICE_DT_GET(DT_INST_PARENT(n)), \
		.reg = DT_REG_ADDR(DT_INST_PARENT(n)),				\
		.channel = DT_INST_PROP(n, channel), \
		/*.reg_size = DT_INST_REG_SIZE(n),*/				\
		/*.init_func = can_rcar_##n##_init,*/				\
		/*CAN_RCAR_INIT_CLOCKS(n),*/ 						\
		/*.clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_IDX(n, 0, name),*/ \
		/*.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)),*/		\
		/*.bus_clk.rate = 40000000, TODO */					\
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),			\
		.configure_irq = can_rcar_rscanfd_configure_irq_##n \
	};									\
	BUILD_ASSERT(DT_INST_PROP(n, channel) < CAN_RCAR_RSCANFD_CHANNELS_COUNT, \
		     "Channel number is invalid."); \
	\
	static struct can_rcar_rscanfd_data can_rcar_rscanfd_data_##n; \
										\
	CAN_DEVICE_DT_INST_DEFINE(n, can_rcar_rscanfd_init,				\
				  NULL,						\
				  &can_rcar_rscanfd_data_##n,				\
				  &can_rcar_rscanfd_config_##n,				\
				  POST_KERNEL,					\
				  CONFIG_CAN_INIT_PRIORITY,			\
				  &can_rcar_rscanfd_driver_api);

DT_INST_FOREACH_STATUS_OKAY(RSCANFD_INIT);

/*
 * The CAN controller.
 */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT renesas_rcar_rscanfd_global

static int can_rcar_rscanfd_global_init(const struct device *dev)
{
	const struct can_rcar_rscanfd_global_config *config = dev->config;
	struct can_rcar_rscanfd_global_data *data = dev->data;
	int ret;

	printk("[%s:%d] entry DRIVER CUSTOM\n", __func__, __LINE__);

	if (!device_is_ready(config->clock_dev)) {
		LOG_ERR("The clock device is not ready.");
		return -ENODEV;
	}

	ret = clock_control_on(config->clock_dev, config->global_clk);
	if (ret != 0) {
		LOG_ERR("Failed to turn the global clock on.");
		return ret;
	}

	/* Make sure that the clock is fast enough for 8Mbit/s CAN-FD */
	ret = clock_control_set_rate(config->clock_dev, config->global_clk,
				     (clock_control_subsys_rate_t)CAN_RCAR_RSCANFD_MODULE_CLOCK_RATE);
	if (ret != 0) {
		LOG_ERR("Failed to set the global clock rate to %u Hz.", CAN_RCAR_RSCANFD_MODULE_CLOCK_RATE);
		return ret;
	}

	ret = clock_control_on(config->clock_dev, config->module_clk);
	if (ret != 0) {
		LOG_ERR("Failed to turn the module clock on.");
		return ret;
	}

	/* Wait for the CAN RAM initialization to terminate */
	ret = can_rcar_rscanfd_busy_wait(config->reg + RSCANFD_CFDGSTS, RSCANFD_CFDGSTS_GRAMINIT, 0);
	if (ret != 0) {
		LOG_ERR("Internal RAM initialization took too long.");
		return ret;
	}

	/* The CAN module registers can be configured only in reset mode */
	ret = can_rcar_rscanfd_enter_reset_mode(config/*, false*/);
	if (ret != 0) {
		LOG_ERR("Failed to enter the reset mode.");
		return ret;
	}

	LOG_DBG("CAN controller IP version: 0x%08X.", sys_read32(config->reg + RSCANFD_CFDGIPV));

	k_mutex_init(&data->list_mutex);

	return 0;
}

#define RSCANFD_GLOBAL_INIT(n)									  \
	static const struct can_rcar_rscanfd_global_config can_rcar_rscanfd_global_config_##n = { \
		.reg = DT_INST_REG_ADDR(n),							  \
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)),				  \
		.global_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_IDX(n, 0, name),	  \
		.module_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_IDX(n, 1, name),	  \
		.num_enabled_channels = DT_INST_CHILD_NUM_STATUS_OKAY(n)			  \
	};											  \
												  \
	static struct can_rcar_rscanfd_global_data can_rcar_rscanfd_global_data_##n;		  \
												  \
	DEVICE_DT_INST_DEFINE(n, can_rcar_rscanfd_global_init,					  \
			 NULL,									  \
			 &can_rcar_rscanfd_global_data_##n,					  \
			 &can_rcar_rscanfd_global_config_##n,					  \
			 POST_KERNEL,								  \
			 CONFIG_KERNEL_INIT_PRIORITY_DEVICE,					  \
			 NULL);

DT_INST_FOREACH_STATUS_OKAY(RSCANFD_GLOBAL_INIT);

/* Make sure that the initialization order will be respected */
BUILD_ASSERT(CONFIG_CLOCK_CONTROL_INIT_PRIORITY < CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
	     "The clocks must be initialized before the CAN controller.");
BUILD_ASSERT(CONFIG_KERNEL_INIT_PRIORITY_DEVICE < CONFIG_CAN_INIT_PRIORITY,
	     "The CAN controller must be initialized before its channels.");
