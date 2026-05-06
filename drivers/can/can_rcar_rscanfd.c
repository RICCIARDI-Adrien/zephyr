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

/* Channel N Nominal Bitrate Configuration Register */
#define RCAR_CAN_CFDCNNCFG 0x0000
/* Channel N Nominal Bitrate Configuration Register bits */
#define RCAR_CAN_CFDCNNCFG_NBRP_MASK 0x3FF
#define RCAR_CAN_CFDCNNCFG_NBRP_SHIFT 0
#define RCAR_CAN_CFDCNNCFG_NSJW_MASK 0x07F
#define RCAR_CAN_CFDCNNCFG_NSJW_SHIFT 10
#define RCAR_CAN_CFDCNNCFG_NTSEG1_MASK 0xFF
#define RCAR_CAN_CFDCNNCFG_NTSEG1_SHIFT 17
#define RCAR_CAN_CFDCNNCFG_NTSEG2_MASK 0xFF
#define RCAR_CAN_CFDCNNCFG_NTSEG2_SHIFT 25

/* Channel n Control Register */
#define RCAR_CAN_CFDCNCTR 0x0004
/* Channel n Control Register bits */
#define RCAR_CAN_CFDCNCTR_CHMDC_MASK 0x03
#define RCAR_CAN_CFDCNCTR_CHMDC_SHIFT 0
#define RCAR_CAN_CFDCNCTR_CHMDC_CHANNEL_OPERATION_REQUEST 0
#define RCAR_CAN_CFDCNCTR_CHMDC_CHANNEL_RESET_REQUEST 0x01
#define RCAR_CAN_CFDCNCTR_CHMDC_CHANNEL_HALT_REQUEST 0x01

/* Channel n Status Register */
#define RCAR_CAN_CFDCNSTS 0x0008
/* Channel n Status Register bits */
#define RCAR_CAN_CFDCNSTS_CSLPSTS BIT(2)
#define RCAR_CAN_CFDCNSTS_CHLTSTS BIT(1)
#define RCAR_CAN_CFDCNSTS_CRSTSTS BIT(0)

/* Global IP Version Register */
#define RCAR_CAN_CFDGIPV 0x0080

/* Global Control Register */
#define RCAR_CAN_CFDGCTR 0x0088
/* Global Control Register bits */
#define RCAR_CAN_CFDGCTR_GMDC_MASK 0x03
#define RCAR_CAN_CFDGCTR_GMDC_SHIFT 0
#define RCAR_CAN_CFDGCTR_GMDC_GLOBAL_OPERATION_MODE_REQUEST 0x00
#define RCAR_CAN_CFDGCTR_GMDC_GLOBAL_RESET_MODE_REQUEST 0x01

/* Global Status Register */
#define RCAR_CAN_CFDGSTS 0x008C
/* Global Status Register bits */
#define RCAR_CAN_CFDGSTS_GRAMINIT BIT(3)
#define RCAR_CAN_CFDGSTS_GRSTSTS BIT(0)

/* Global Acceptance Filter List Entry Control Register */
#define RCAR_CAN_CFDGAFLECTR 0x0098
#define RCAR_CAN_CFDGAFLECTR_AFLDAE BIT(8)

/* Global Acceptance Filter List Configuration Register n */
#define RCAR_CAN_CFDGAFLCFGN 0x009C
/* Global Acceptance Filter List Configuration Register n bits */
#define RCAR_CAN_CFDGAFLCFGN_RNC_CHANNEL_MASK 0x001F
#define RCAR_CAN_CFDGAFLCFGN_RNC_CHANNEL_EVEN_SHIFT 16
#define RCAR_CAN_CFDGAFLCFGN_RNC_CHANNEL_ODD_SHIFT 0

/* RX Message Buffer Number Register */
#define RCAR_CAN_CFDRMNB 0x00AC

/* RX FIFO Configuration / Control Register N */
#define RCAR_CAN_CFDRFCCN 0x00C0
/* RX FIFO Configuration / Control Register N bits */
#define RCAR_CAN_CFDRFCCN_RFDC_MASK 0x07
#define RCAR_CAN_CFDRFCCN_RFDC_SHIFT 8
#define RCAR_CAN_CFDRFCCN_RFDC_DEPTH_64 0x06
#define RCAR_CAN_CFDRFCCN_RFDC_DEPTH_128 0x07
#define RCAR_CAN_CFDRFCCN_RFPLS_MASK 0x07
#define RCAR_CAN_CFDRFCCN_RFPLS_SHIFT 4
#define RCAR_CAN_CFDRFCCN_RFPLS_SIZE_64 0x07
#define RCAR_CAN_CFDRFCCN_RFE_MASK 0x01
#define RCAR_CAN_CFDRFCCN_RFE_SHIFT 0
#define RCAR_CAN_CFDRFCCN_RFE_DISABLE 0
#define RCAR_CAN_CFDRFCCN_RFE_ENABLE 1

/* Common FIFO Configuration / Control Register N */
#define RCAR_CAN_CFDCFCCN 0x0120
/* Common FIFO Configuration / Control Register N bits */
#define RCAR_CAN_CFDCFCCN_CFDC_MASK 0x03
#define RCAR_CAN_CFDCFCCN_CFDC_SHIFT 21
#define RCAR_CAN_CFDCFCCN_CFDC_DEPTH_64 0x06
#define RCAR_CAN_CFDCFCCN_CFDC_DEPTH_128 0x07
#define RCAR_CAN_CFDCFCCN_CFIM_SHIFT 12
#define RCAR_CAN_CFDCFCCN_CFIM_TX_EVERY_MESSAGE 1
#define RCAR_CAN_CFDCFCCN_CFIM_TX_LAST_MESSAGE 0 // TODO moins d'interrupts mais gestion queue de tx callbacks, voir plus tard pour perfs
#define RCAR_CAN_CFDCFCCN_CFM_MASK 0x03
#define RCAR_CAN_CFDCFCCN_CFM_SHIFT 8
#define RCAR_CAN_CFDCFCCN_CFM_TX 0x01
#define RCAR_CAN_CFDCFCCN_CFPLS_SHIFT 4
#define RCAR_CAN_CFDCFCCN_CFPLS_SIZE_64 0x07
#define RCAR_CAN_CFDCFCCN_CFTXIE_SHIFT 2
#define RCAR_CAN_CFDCFCCN_CFTXIE_INT_ENABLED 1
#define RCAR_CAN_CFDCFCCN_CFE_MASK 0x01
#define RCAR_CAN_CFDCFCCN_CFE_SHIFT 0
#define RCAR_CAN_CFDCFCCN_CFE_DISABLE 0
#define RCAR_CAN_CFDCFCCN_CFE_ENABLE 1

/* Common FIFO Configuration / Control Enhancement Register N */
#define RCAR_CAN_CFDCFCCEN 0x0180
/* Common FIFO Configuration / Control Enhancement Register N bits */
#define RCAR_CAN_CFDCFCCEN_CFBME_SHIFT 16
#define RCAR_CAN_CFDCFCCEN_CFBME_ENABLE 0

/* Common FIFO Status Registers N */
#define RCAR_CAN_CFDCFSTSN 0x01E0
/* Common FIFO Status Registers N bits */
#define RCAR_CAN_CFDCFSTSN_CFTXIF BIT(4)

/* Common FIFO Pointer Control Register 0 */
#define RCAR_CAN_CFDCFPCTR0 0x0240

/* Global Acceptance Filter List ID Register N */
#define RCAR_CAN_CFDGAFLIDN 0x1800

/* Global Acceptance Filter List Mask Register N */
#define RCAR_CAN_CFDGAFLMN 0x1804

/* Global Acceptance Filter List Pointer 0 Register N */
#define RCAR_CAN_CFDGAFLP0N 0x1808

/* Global Acceptance Filter List Pointer 1 Register N */
#define RCAR_CAN_CFDGAFLP1N 0x180C

/* Common FIFO Access Message Buffer Component */
#define RCAR_CAN_CFDCFMBCP0 0x6400 // TODO virer
/* Common FIFO Access ID Registers 0 N */
#define RCAR_CAN_CFDCFID0N 0x6400
/* Common FIFO Access ID Registers 0 N bits */
#define RCAR_CAN_CFDCFID0N_CFRTR BIT(31)
#define RCAR_CAN_CFDCFID0N_CFIDE BIT(30)
#define RCAR_CAN_CFDCFID0N_CFID_MASK 0x1FFFFFFF
#define RCAR_CAN_CFDCFID0N_CFID_SHIFT 0

/* Common FIFO Access Pointer Registers 0 N */
#define RCAR_CAN_CFDCFPTR0N 0x6404
/* Common FIFO Access Pointer Registers 0 N bits */
#define RCAR_CAN_CFDCFPTR0N_CFDLC_MASK 0x0F
#define RCAR_CAN_CFDCFPTR0N_CFDLC_SHIFT 28

/* Common FIFO Access CAN-FD Control/Status Register 0 N */
#define RCAR_CAN_CFDCFFDCSTS0N 0x6408
/* Common FIFO Access CAN-FD Control/Status Register 0 N bits */
#define RCAR_CAN_CFDCFFDCSTS0N_CFFDF BIT(2)
#define RCAR_CAN_CFDCFFDCSTS0N_CFBRS BIT(1)
#define RCAR_CAN_CFDCFFDCSTS0N_CFESI BIT(0)

/* Common FIFO Access Data Field 0 Registers 0 N */
#define RCAR_CAN_CFDCFDF00N 0x640C

#define RCAR_CAN_FIFO_ACCESS_DATA_REGISTERS_COUNT 16

#define RCAR_CAN_RSCANFD_MODULE_CLOCK_RATE 80000000

/** The amount of channels per controller. */
#define RCAR_CAN_RSCANFD_CHANNELS_COUNT 8

/** How many Common FIFO are assigned to each channel. */
#define RCAR_CAN_RSCANFD_COMMON_FIFO_PER_CHANNEL 3

struct can_rcar_rscanfd_global_cfg {
	uint32_t reg;
	const struct device *clock_dev;
	// TODO utiliser API clock générique
	clock_control_subsys_t global_clk;
	clock_control_subsys_t module_clk;
	uint32_t num_enabled_channels;
};

struct can_rcar_rscanfd_cfg {
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
};

typedef struct
{
	uint32_t id;
	uint32_t dlc_timestamp;
	uint32_t ctrl_status;
	uint32_t data[RCAR_CAN_FIFO_ACCESS_DATA_REGISTERS_COUNT];
	//uint8_t data[RCAR_CAN_FIFO_ACCESS_DATA_REGISTERS_COUNT * 4];
} __packed rcar_msg_buffer_component_t;

/**
 * TODO
 *
 * @note This function can sleep, do not use it in interrupt context.
 **/
static inline int can_rscanfd_busy_wait(mem_addr_t reg, uint32_t bit_mask, bool expect_one)
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
static int can_rcar_rscanfd_enter_reset_mode(const struct can_rcar_rscanfd_global_cfg *config/*, bool force*/)
{
	/* Request the global reset mode, resetting all other fields in the same time */
	sys_write32(RCAR_CAN_CFDGCTR_GMDC_GLOBAL_RESET_MODE_REQUEST, config->reg + RCAR_CAN_CFDGCTR);

	return can_rscanfd_busy_wait(config->reg + RCAR_CAN_CFDGSTS, RCAR_CAN_CFDGSTS_GRSTSTS, 1);
}

/**
 * TODO
 * @note The halt state is applied at the channel level, not impacting the other channels.
 */
static int can_rcar_rscanfd_enter_halt_mode(const struct can_rcar_rscanfd_cfg *config)
{
#if 0
	uint32_t base_addr, val;

	// TODO

	/* The relevant registers are 4-byte wide and consecutive */
	base_addr = config->reg + (config->channel * 4);

	val = sys_read32(base_addr + RCAR_CAN_CFDCNCTR);
	val &= ~(RCAR_CAN_CFDCNCTR_CHMDC_MASK << RCAR_CAN_CFDCNCTR_CHMDC_SHIFT);
	val |= RCAR_CAN_CFDCNCTR_CHMDC_CHANNEL_HALT_REQUEST << RCAR_CAN_CFDCNCTR_CHMDC_SHIFT;
	sys_write32(val, base_addr + RCAR_CAN_CFDCNCTR);

	return can_rscanfd_busy_wait(base_addr + RCAR_CAN_CFDCNSTS, RCAR_CAN_CFDCNSTS_CHLTSTS, 1);
#endif
	return 0;
}

/**
 * TODO
 * @note The operation state is applied at controller level, impacting all channels.
 */
static int can_rcar_rscanfd_enter_operation_mode(const struct can_rcar_rscanfd_global_cfg *config)
{
	uint32_t val;

	val = sys_read32(config->reg + RCAR_CAN_CFDGCTR);
	val &= ~(RCAR_CAN_CFDGCTR_GMDC_MASK << RCAR_CAN_CFDGCTR_GMDC_SHIFT);
	val |= RCAR_CAN_CFDGCTR_GMDC_GLOBAL_OPERATION_MODE_REQUEST << RCAR_CAN_CFDGCTR_GMDC_SHIFT;
	sys_write32(val, config->reg + RCAR_CAN_CFDGCTR);

	return can_rscanfd_busy_wait(config->reg + RCAR_CAN_CFDGSTS, RCAR_CAN_CFDGSTS_GRSTSTS, 0);
}

static int can_rcar_rscanfd_channel_leave_sleep_mode(const struct can_rcar_rscanfd_cfg *config)
{
	uint32_t base_addr = config->reg + (config->channel * 16);
	int ret;

	/* Release the channel from sleep mode, resetting all other fields in the same time. */
	//printk("[%s:%d] entry RCAR_CAN_CFDCNCTR avant = 0x%08X\n", __func__, __LINE__, sys_read32(base_addr + RCAR_CAN_CFDCNCTR));
	//printk("[%s:%d] entry RCAR_CAN_CFDCNSTS avant = 0x%08X\n", __func__, __LINE__, sys_read32(base_addr + RCAR_CAN_CFDCNSTS));
	sys_write32(RCAR_CAN_CFDCNCTR_CHMDC_CHANNEL_RESET_REQUEST << RCAR_CAN_CFDCNCTR_CHMDC_SHIFT,
		    base_addr + RCAR_CAN_CFDCNCTR);

	ret = can_rscanfd_busy_wait(base_addr + RCAR_CAN_CFDCNSTS, RCAR_CAN_CFDCNSTS_CSLPSTS, 0);
	if (ret != 0) {
		LOG_ERR("Leaving the sleep mode for channel %u took too long.", config->channel);
		return ret;
	}

	return 0;
}

static int can_rcar_rscanfd_channel_enter_operation_mode(const struct can_rcar_rscanfd_cfg *config)
{
	uint32_t base_addr = config->reg + (config->channel * 16), val;
	int ret;

	/* Determine the current channel status */
	/*val = sys_read32(base_addr + RCAR_CAN_CFDCNSTS);
	if (val & RCAR_CAN_CFDCNSTS_CRSTSTS) {
		status_bit = RCAR_CAN_CFDCNSTS_CRSTSTS*/

	val = sys_read32(base_addr + RCAR_CAN_CFDCNCTR);
	val &= ~(RCAR_CAN_CFDCNCTR_CHMDC_MASK << RCAR_CAN_CFDCNCTR_CHMDC_SHIFT);
	val |= RCAR_CAN_CFDCNCTR_CHMDC_CHANNEL_OPERATION_REQUEST << RCAR_CAN_CFDCNCTR_CHMDC_SHIFT;
	sys_write32(val, base_addr + RCAR_CAN_CFDCNCTR);

	ret = can_rscanfd_busy_wait(base_addr + RCAR_CAN_CFDCNSTS, RCAR_CAN_CFDCNSTS_CRSTSTS, 0);
	if (ret != 0) {
		LOG_ERR("Going from reset to operation mode for channel %u took too long.",
			config->channel);
		return ret;
	}
	ret = can_rscanfd_busy_wait(base_addr + RCAR_CAN_CFDCNSTS, RCAR_CAN_CFDCNSTS_CHLTSTS, 0);
	if (ret != 0) {
		LOG_ERR("Going from halt to operation mode for channel %u took too long.",
			config->channel);
		return ret;
	}
	// TODO sleep ?

	/* Wait for the controller to apply the new state */
	/*for (int i = 0; i < MAX_STR_READS; i++) {
		if (!(sys_read32(config->reg_addr + RCAR_CAN_CFDCNSTS) & RCAR_CAN_CFDCNSTS_CSLPSTS)) {
			printk("[%s:%d] RCAR_CAN_CFDCNCTR OK\n", __func__, __LINE__);
			return 0;
		}
	}*/
	//printk("[%s:%d] entry RCAR_CAN_CFDCNSTS apres = 0x%08X\n", __func__, __LINE__, sys_read32(base_addr + RCAR_CAN_CFDCNSTS));

	return 0;
}

/**
 * TODO
 * Configure the rules table by creating one rule that matches them all (reception frames).
 * The reception filters are currently implemented in software.
 */
static void can_rcar_rscanfd_configure_acceptance_filter_list(const struct can_rcar_rscanfd_cfg *config)
{
	uint32_t addr, val, shift;

	/*
	 * Enable write access for the page 0 (set the page index 0 value in the same time).
	 * As all channels will use a single rule, only the first page is needed.
	 */
	sys_write32(RCAR_CAN_CFDGAFLECTR_AFLDAE, config->reg + RCAR_CAN_CFDGAFLECTR);

	/* There are two consecutive channel rules per CFDGAFLCFGn register */
	addr = config->reg + RCAR_CAN_CFDGAFLCFGN + (config->channel / 2);
	/* Address the correct channel within the register */
	val = sys_read32(addr);
	//printk("[%s:%d] RCAR_CAN_CFDGAFLCFG%d avant = 0x%08X\n", __func__, __LINE__, config->channel, sys_read32(addr));
	if (config->channel & 1) { /* Odd channel */
		shift = RCAR_CAN_CFDGAFLCFGN_RNC_CHANNEL_ODD_SHIFT;
	}
	else {
		shift = RCAR_CAN_CFDGAFLCFGN_RNC_CHANNEL_EVEN_SHIFT;
	}
	val &= ~(RCAR_CAN_CFDGAFLCFGN_RNC_CHANNEL_MASK << shift);
	/* Configure one rule for the channel */
	val |= 1 << shift;
	sys_write32(val, addr);
	//printk("[%s:%d] RCAR_CAN_CFDGAFLCFG%d apres = 0x%08X\n", __func__, __LINE__, config->channel, sys_read32(addr));

	/* A page contains 16 entries, with a 4-byte register per entry */
	addr = config->reg + (config->channel * 4);
	/* Clear the CAN ID as it won't be taken into account by the mask register */
	sys_write32(0, addr + RCAR_CAN_CFDGAFLIDN);
	/* Accept all received CAN frames */
	sys_write32(0, addr + RCAR_CAN_CFDGAFLMN);
	/* Disable the DLC check */
	sys_write32(0, addr + RCAR_CAN_CFDGAFLP0N);
	/* Use a dedicated RX FIFO as target for reception */
	sys_write32(1 << config->channel, addr + RCAR_CAN_CFDGAFLP1N);

	/* Disable write access for page 0 */
	sys_write32(0, config->reg + RCAR_CAN_CFDGAFLECTR);
}

static void can_rcar_rscanfd_configure_fifo(const struct can_rcar_rscanfd_cfg *config)
{
	uint32_t base_addr;

	/*
	 * All relevant registers are 32-bit wide and consecutive.
	 * We are using the first Common FIFO from the several available for each channel.
	 */
	base_addr = config->reg +
		(config->channel * sizeof(uint32_t) * RCAR_CAN_RSCANFD_COMMON_FIFO_PER_CHANNEL);

	/* Dedicate a RX FIFO to the channel */
	sys_write32(RCAR_CAN_CFDRFCCN_RFDC_DEPTH_64 << RCAR_CAN_CFDRFCCN_RFDC_SHIFT |
		RCAR_CAN_CFDRFCCN_RFPLS_SIZE_64 << RCAR_CAN_CFDRFCCN_RFPLS_SHIFT |
		RCAR_CAN_CFDRFCCN_RFE_DISABLE << RCAR_CAN_CFDRFCCN_RFE_SHIFT,
		base_addr + RCAR_CAN_CFDRFCCN); // TODO Message lost Error Interrupt Enable ? RX FIFO Interrupt Mode ? etc int

	/* Dedicate a Common FIFO for transmission */
	sys_write32(RCAR_CAN_CFDCFCCN_CFDC_DEPTH_64 << RCAR_CAN_CFDCFCCN_CFDC_SHIFT |
		RCAR_CAN_CFDCFCCN_CFIM_TX_EVERY_MESSAGE << RCAR_CAN_CFDCFCCN_CFIM_SHIFT |
		RCAR_CAN_CFDCFCCN_CFM_TX << RCAR_CAN_CFDCFCCN_CFM_SHIFT |
		RCAR_CAN_CFDCFCCN_CFPLS_SIZE_64 << RCAR_CAN_CFDCFCCN_CFPLS_SHIFT |
		RCAR_CAN_CFDCFCCN_CFTXIE_INT_ENABLED << RCAR_CAN_CFDCFCCN_CFTXIE_SHIFT |
		RCAR_CAN_CFDCFCCN_CFE_DISABLE << RCAR_CAN_CFDCFCCN_CFE_SHIFT,
		base_addr + RCAR_CAN_CFDCFCCN); // TODO transmission delay dans param DT ? int

	/* Allow transmitting from the Common FIFO */
	sys_write32(RCAR_CAN_CFDCFCCEN_CFBME_ENABLE << RCAR_CAN_CFDCFCCEN_CFBME_SHIFT,
		base_addr + RCAR_CAN_CFDCFCCEN);
}

/**
 * TODO
 *
 * @note The channel must already be in Reset or Halt state.
 */
static int can_rcar_rscanfd_configure_timing(const struct can_rcar_rscanfd_cfg *config,
					     const struct can_timing *timing)
{
	uint32_t base_addr;

	LOG_DBG("Set timing for channel %u, sjw=%u, prop_seg=%u, seg1=%u, seg2=%u, presc=%u.",
		config->channel, timing->sjw, timing->prop_seg, timing->phase_seg1, timing->phase_seg2,
		timing->prescaler);

	/* Each channel is handled by a group of four 4-byte registers */
	base_addr = config->reg + (config->channel * 16);

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
	sys_write32((timing->phase_seg1 - 1) << RCAR_CAN_CFDCNNCFG_NTSEG1_SHIFT |
		(timing->phase_seg2 - 1) << RCAR_CAN_CFDCNNCFG_NTSEG2_SHIFT |
		timing->sjw << RCAR_CAN_CFDCNNCFG_NSJW_SHIFT |
		(timing->prescaler - 1) << RCAR_CAN_CFDCNNCFG_NBRP_SHIFT,
		base_addr + RCAR_CAN_CFDCNNCFG);

	// TODO CFDCnCTR

	//printk("[%s:%d] CFDCnNCFG %u = 0x%08X\n", __func__, __LINE__, config->channel, sys_read32(base_addr + RCAR_CAN_CFDCNNCFG));

	return 0;
}

static int can_rscanfd_get_capabilities(const struct device *dev, can_mode_t *cap)
{
	ARG_UNUSED(dev);

	printk("[%s:%d] entry\n", __func__, __LINE__);
	// TODO
	*cap = CAN_MODE_NORMAL | CAN_MODE_LOOPBACK | CAN_MODE_LISTENONLY;

	return 0;
}

static int can_rscanfd_start(const struct device *dev)
{
	const struct can_rcar_rscanfd_cfg *config = dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	uint32_t base_addr, val;
	int ret;

	// activ FIFOs
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

	CAN_STATS_RESET(dev);

	ret = can_rcar_rscanfd_channel_enter_operation_mode(config);
	if (ret != 0) {
		return ret;
	}

	/* All relevant registers are 32-bit and consecutive */
	base_addr = config->reg + (config->channel * sizeof(uint32_t));

	/* The FIFOs can be enabled only when the channel is in operation mode */
	/* Reception FIFO */
	val = sys_read32(base_addr + RCAR_CAN_CFDRFCCN);
	val &= ~(RCAR_CAN_CFDRFCCN_RFE_MASK << RCAR_CAN_CFDRFCCN_RFE_SHIFT);
	val |= RCAR_CAN_CFDRFCCN_RFE_ENABLE << RCAR_CAN_CFDRFCCN_RFE_SHIFT;
	sys_write32(val, base_addr + RCAR_CAN_CFDRFCCN);
	/* Transmission FIFO */
	val = sys_read32(base_addr + RCAR_CAN_CFDCFCCN);
	val &= ~(RCAR_CAN_CFDCFCCN_CFE_MASK << RCAR_CAN_CFDCFCCN_CFE_SHIFT);
	val |= RCAR_CAN_CFDCFCCN_CFE_ENABLE << RCAR_CAN_CFDCFCCN_CFE_SHIFT;
	sys_write32(val, base_addr + RCAR_CAN_CFDCFCCN);

	data->common.started = true;

	// TEST
	printk("[%s:%d] CFDCNNCFG0=0x%08X, CFDCNNCFG1=0x%08X, CFDCNCTR0=0x%08X, CFDCNCTR1=0x%08X, CFDCNSTS0=0x%08X, CFDCNSTS1=0x%08X, CFDCFCCN0=0x%08X, CFDCFCCN1=0x%08X, RCAR_CAN_CFDCFCCEN0=0x%08X, RCAR_CAN_CFDCFCCEN1=0x%08X, CFDCFSTS0=0x%08X, CFDCFSTS1=0x%08X\n", __func__, __LINE__, sys_read32(config->reg), sys_read32(config->reg + 16), sys_read32(config->reg + 4), sys_read32(config->reg + 4 + 16), sys_read32(config->reg + 8), sys_read32(config->reg + 8 + 16), sys_read32(config->reg + 0x120), sys_read32(config->reg + 0x120 + (3*4)), sys_read32(config->reg + 0x180), sys_read32(config->reg + 0x180 + (3*4)), sys_read32(config->reg + 0x1E0), sys_read32(config->reg + 0x1E0 + (3*4)));


#if 0
	const struct rscanfd_cfg *config = dev->config;
	struct can_rcar_data *data = dev->data;
	int ret;

	printk("[%s:%d] entry\n", __func__, __LINE__);

	if (data->common.started) {
		return -EALREADY;
	}


	k_mutex_lock(&data->inst_mutex, K_FOREVER);

	CAN_STATS_RESET(dev);

	ret = can_rcar_enter_operation_mode(config);
	if (ret != 0) {
		LOG_ERR("failed to enter operation mode (err %d)", ret);

		if (config->common.phy != NULL) {
			/* Attempt to disable the CAN transceiver in case of error */
			(void)can_transceiver_disable(config->common.phy);
		}
	} else {
		data->common.started = true;
	}

	k_mutex_unlock(&data->inst_mutex);

	return ret;
#endif
	return 0;
}

static int can_rscanfd_stop(const struct device *dev)
{
	printk("[%s:%d] entry\n", __func__, __LINE__);
	// TODO
	return 0;
}

static int can_rcar_rscanfd_set_mode(const struct device *dev, can_mode_t mode)
{
	const struct can_rcar_rscanfd_cfg *config = dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;

	LOG_DBG("Set mode %u for channel %u (current mode is %u).",
		mode, config->channel, data->common.mode);

	// TODO vérif état stopped, sinon -EBUSY

	// TODO manual mode

	// TODO check supported modes

	// TODO variable mode dans data dev
	data->common.mode = mode;

	return 0;
}

static int can_rcar_rscanfd_set_timing(const struct device *dev, const struct can_timing *timing)
{
	const struct can_rcar_rscanfd_cfg *config = dev->config;
	int ret;

	// TODO verif init OK ?

	// TODO reset mode, avec

	ret = can_rcar_rscanfd_enter_halt_mode(config); // TODO
	if (ret != 0) {
		return -EIO;
	}

	// TODO
	return 0;
}

static int can_rscanfd_send(const struct device *dev, const struct can_frame *frame,
			    k_timeout_t timeout, can_tx_callback_t callback,
			    void *user_data)
{
	const struct can_rcar_rscanfd_cfg *config = dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	uint32_t base_addr, val, i;

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

	base_addr = config->reg + (config->channel * 0x180);

	if (k_sem_take(&data->tx_sem, timeout) != 0) {
		return -EAGAIN;
	}

	k_mutex_lock(&data->inst_mutex, K_FOREVER);

	data->tx_callback = callback;
	data->tx_user_data = user_data;

	/* ID and flags */
	val = (frame->id & RCAR_CAN_CFDCFID0N_CFID_MASK) << RCAR_CAN_CFDCFID0N_CFID_SHIFT;
	if (frame->flags & CAN_FRAME_IDE) {
		val |= RCAR_CAN_CFDCFID0N_CFIDE;
	}
	if (frame->flags & CAN_FRAME_RTR) {
		val |= RCAR_CAN_CFDCFID0N_CFRTR;
	}
	sys_write32(val, base_addr + RCAR_CAN_CFDCFID0N);

	/* DLC and timestamp TODO */
	sys_write32((frame->dlc & RCAR_CAN_CFDCFPTR0N_CFDLC_MASK) << RCAR_CAN_CFDCFPTR0N_CFDLC_SHIFT,
		base_addr + RCAR_CAN_CFDCFPTR0N);

	/* CAN-FD settings */
	val = 0;
	if (frame->flags & CAN_FRAME_FDF) {
		val |= RCAR_CAN_CFDCFFDCSTS0N_CFFDF;
	}
	if (frame->flags & CAN_FRAME_BRS) {
		val |= RCAR_CAN_CFDCFFDCSTS0N_CFBRS;
	}
	if (frame->flags & CAN_FRAME_ESI) {
		val |= RCAR_CAN_CFDCFFDCSTS0N_CFESI;
	}
	sys_write32(val, base_addr + RCAR_CAN_CFDCFFDCSTS0N);

	/* Data */
	val = can_dlc_to_bytes(frame->dlc);
	for (i = 0; i < val; i++) {
		sys_write8(frame->data[i], base_addr + RCAR_CAN_CFDCFDF00N + i);
	}

	// TEST
	/*sys_write32(0x2CA, base_addr + RCAR_CAN_CFDCFID0N);
	sys_write32(8 << 28, base_addr + RCAR_CAN_CFDCFMBCP0 + 4);
	sys_write32(0, base_addr + RCAR_CAN_CFDCFMBCP0 + 8);
	sys_write32(0xBABEB00B, base_addr + RCAR_CAN_CFDCFMBCP0 + 12);
	sys_write32(0xDEADBEEF, base_addr + RCAR_CAN_CFDCFMBCP0 + 16);*/

	sys_write32(0xFF, base_addr + RCAR_CAN_CFDCFPCTR0);

	k_mutex_unlock(&data->inst_mutex);

	printk("[%s:%d] transmission OK\n", __func__, __LINE__);

	// TODO
	return 0;
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
	const struct can_rcar_rscanfd_cfg *config = dev->config;
	const struct can_rcar_rscanfd_global_cfg *global_config = config->global_dev->config;

	//printk("[%s:%d] config->clk=%p\n", __func__, __LINE__, global_config->global_clk);
	return clock_control_get_rate(global_config->clock_dev, global_config->global_clk, rate);
}

static int can_rscanfd_get_max_filters(const struct device *dev, bool ide)
{
	ARG_UNUSED(ide);

	printk("[%s:%d] entry\n", __func__, __LINE__);

	// TODO
	return /*CONFIG_CAN_RCAR_MAX_FILTERS*/10;
}

static void can_rcar_rscanfd_isr(const struct device *dev)
{
	const struct can_rcar_rscanfd_cfg *config = dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	uint32_t base_addr, irq_flags;

	printk("[%s:%d] entry dev=%s\n", __func__, __LINE__, dev->name);

	/* The status registers are all consecutive */
	base_addr = config->reg + (config->channel * sizeof(uint32_t));

	irq_flags = sys_read32(base_addr + RCAR_CAN_CFDCFSTSN);
	if (irq_flags & RCAR_CAN_CFDCFSTSN_CFTXIF) {
		data->tx_callback(dev, 0 /* TODO */, data->tx_user_data);
		k_sem_give(&data->tx_sem);

		/* Clear the interrupt flag */
		irq_flags &= ~RCAR_CAN_CFDCFSTSN_CFTXIF;
		sys_write32(irq_flags, base_addr + RCAR_CAN_CFDCFSTSN);
	}
}

static int can_rcar_rscanfd_init(const struct device *dev)
{
	static uint32_t enabled_channels_count = 0;
	const struct can_rcar_rscanfd_cfg *config = dev->config;
	const struct can_rcar_rscanfd_global_cfg *global_config = config->global_dev->config;
	struct can_rcar_rscanfd_data *data = dev->data;
	struct can_timing timing /*= {0}*/;
	int ret;

	printk("[%s:%d] entry DRIVER CUSTOM CHANNEL %d\n", __func__, __LINE__, config->channel);

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

	ret = can_rcar_rscanfd_set_mode(dev, CAN_MODE_NORMAL);
	if (ret) {
		return ret;
	}

	// TEST
	//}

	config->configure_irq();

	k_mutex_init(&data->inst_mutex);
	k_sem_init(&data->tx_sem, 1, 1);

	/* Switch the controller to operation mode when all channels are initialized */
	enabled_channels_count++;
	if (enabled_channels_count == global_config->num_enabled_channels) {
		ret = can_rcar_rscanfd_enter_operation_mode(global_config);
		if (ret) {
			LOG_ERR("Failed to put the controller in operation mode.");
			// TODO uninit ?
			return ret;
		}
		LOG_DBG("All channels were successfully initialized.");
	}

	return 0;
}

static DEVICE_API(can, can_rcar_rscanfd_driver_api) = {
	.get_capabilities = can_rscanfd_get_capabilities,
	.start = can_rscanfd_start,
	.stop = can_rscanfd_stop,
	.set_mode = can_rcar_rscanfd_set_mode,
	.set_timing = can_rcar_rscanfd_set_timing,
	.send = can_rscanfd_send,
	//.add_rx_filter = can_rcar_add_rx_filter,
	//.remove_rx_filter = can_rcar_remove_rx_filter,
	.get_state = can_rscanfd_get_state,
/*#ifdef CONFIG_CAN_MANUAL_RECOVERY_MODE
	.recover = can_rcar_recover,
#endif*/ /* CONFIG_CAN_MANUAL_RECOVERY_MODE */
	//.set_state_change_callback = can_rcar_set_state_change_callback,
	.get_core_clock = can_rcar_rscanfd_get_core_clock,
	.get_max_filters = can_rscanfd_get_max_filters,
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
#define CAN_RCAR_RSCANFD_INIT(n)							\
	PINCTRL_DT_INST_DEFINE(n);						\
	\
	static void can_rcar_rscandfd_configure_irq_##n(void) \
	{ \
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority), \
			    can_rcar_rscanfd_isr, DEVICE_DT_INST_GET(n), 0); \
		irq_enable(DT_INST_IRQN(n)); \
	} \
	\
	/*static void can_rcar_##n##_init(const struct device *dev);*/		\
	static const struct can_rcar_rscanfd_cfg can_rcar_rscanfd_cfg_##n = {			\
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
		.configure_irq = can_rcar_rscandfd_configure_irq_##n \
	};									\
	BUILD_ASSERT(DT_INST_PROP(n, channel) < RCAR_CAN_RSCANFD_CHANNELS_COUNT, \
		     "Channel number is invalid."); \
	/*static struct can_rcar_data can_rcar_data_##n;*/				\
	\
	static struct can_rcar_rscanfd_data can_rcar_rscanfd_data_##n; \
										\
	CAN_DEVICE_DT_INST_DEFINE(n, can_rcar_rscanfd_init,				\
				  NULL,						\
				  &can_rcar_rscanfd_data_##n,				\
				  &can_rcar_rscanfd_cfg_##n,				\
				  POST_KERNEL,					\
				  CONFIG_CAN_INIT_PRIORITY,			\
				  &can_rcar_rscanfd_driver_api);				/*\
				  						\
	static void can_rcar_##n##_init(const struct device *dev)		\
	{									\
		IRQ_CONNECT(DT_INST_IRQN(n),					\
			    0,							\
			    can_rcar_isr,					\
			    DEVICE_DT_INST_GET(n), 0);				\
										\
		irq_enable(DT_INST_IRQN(n));					\
	}*/

DT_INST_FOREACH_STATUS_OKAY(CAN_RCAR_RSCANFD_INIT);

/*
 * The CAN controller.
 */
#undef DT_DRV_COMPAT
#define DT_DRV_COMPAT renesas_rcar_rscanfd_global

static int can_rcar_rscanfd_global_init(const struct device *dev)
{
	const struct can_rcar_rscanfd_global_cfg *config = dev->config;
	//struct can_rcar_data *data = dev->data;
	//struct can_timing timing = { 0 };
	int ret;

	printk("[%s:%d] entry DRIVER CUSTOM\n", __func__, __LINE__);

	/*k_mutex_init(&data->inst_mutex);
	k_mutex_init(&data->rx_mutex);
	k_sem_init(&data->tx_sem, RCAR_CAN_FIFO_DEPTH, RCAR_CAN_FIFO_DEPTH);

	data->tx_head = 0;
	data->tx_tail = 0;
	data->tx_unsent = 0;

	memset(data->rx_callback, 0, sizeof(data->rx_callback));
	data->state = CAN_STATE_ERROR_ACTIVE;
	data->common.state_change_cb = NULL;
	data->common.state_change_cb_user_data = NULL;

	if (config->common.phy != NULL && !device_is_ready(config->common.phy)) {
		printk("[%s:%d] err\n", __func__, __LINE__);
		LOG_ERR("CAN transceiver not ready");
		return -ENODEV;
	}*/

	if (!device_is_ready(config->clock_dev)) {
		LOG_ERR("The clock device is not ready.");
		return -ENODEV;
	}

	/* Configure dt provided device signals when available */
	/*ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret != 0) {
		LOG_ERR("Failed to apply the pinctrl state.");
		return ret;
	}*/

	/* Reset the registers */
	/*printk("[%s:%d] config->clock_dev=%p, config->clk=%p\n", __func__, __LINE__, config->clock_dev, config->clk);
	ret = clock_control_off(config->clock_dev, config->clk);
	if (ret != 0) {
		printk("[%s:%d] err ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}*/

	ret = clock_control_on(config->clock_dev, config->global_clk);
	if (ret != 0) {
		LOG_ERR("Failed to turn the global clock on.");
		return ret;
	}

	// TEST
	/*{
		uint32_t rate;
		ret = clock_control_get_rate(config->clock_dev, config->global_clk, &rate);
		if (ret != 0) printk("[%s:%d] err rate clock %d\n",  __func__, __LINE__, ret);
		else printk("[%s:%d] rate clock avant %u\n",  __func__, __LINE__, rate);
	}*/

	/* Make sure that the clock is fast enough for 8Mbit/s CAN-FD */
	ret = clock_control_set_rate(config->clock_dev, config->global_clk,
				     (clock_control_subsys_rate_t)RCAR_CAN_RSCANFD_MODULE_CLOCK_RATE);
	if (ret != 0) {
		LOG_ERR("Failed to set the global clock rate to %u Hz.", RCAR_CAN_RSCANFD_MODULE_CLOCK_RATE);
		return ret;
	}

	// TEST
	/*{
		uint32_t rate;
		ret = clock_control_get_rate(config->clock_dev, config->global_clk, &rate);
		if (ret != 0) printk("[%s:%d] err rate clock %d\n",  __func__, __LINE__, ret);
		else printk("[%s:%d] rate clock apres %u\n",  __func__, __LINE__, rate);
	}*/

	ret = clock_control_on(config->clock_dev, config->module_clk);
	if (ret != 0) {
		LOG_ERR("Failed to turn the module clock on.");
		return ret;
	}

	/* Wait for the CAN RAM initialization to terminate */
	//printk("[%s:%d] avant init RAM\n",  __func__, __LINE__);
	ret = can_rscanfd_busy_wait(config->reg + RCAR_CAN_CFDGSTS, RCAR_CAN_CFDGSTS_GRAMINIT, 0);
	if (ret != 0) {
		LOG_ERR("Internal RAM initialization took too long.");
		return ret;
	}
	//printk("[%s:%d] apres init RAM\n",  __func__, __LINE__);

	/* The CAN module registers can be configured only in reset mode */
	ret = can_rcar_rscanfd_enter_reset_mode(config/*, false*/);
	if (ret != 0) {
		LOG_ERR("Failed to enter the reset mode.");
		return ret;
	}

	LOG_DBG("CAN controller IP version: 0x%08X.", sys_read32(config->reg + RCAR_CAN_CFDGIPV));

#if 0
	ret = can_rcar_leave_sleep_mode(config);
	__ASSERT(!ret, "Fail to leave CAN controller from sleep mode");
	if (ret) {
		printk("[%s:%d] err ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}

	printk("[%s:%d] config->common.bitrate=%u, config->common.sample_point=%u\n", __func__, __LINE__, config->common.bitrate, config->common.sample_point);
	ret = can_calc_timing(dev, &timing, config->common.bitrate,
			      config->common.sample_point);
	if (ret == -EINVAL) {
		LOG_ERR("Can't find timing for given param");
		printk("[%s:%d] err ret=%d\n", __func__, __LINE__, -EIO);
		return -EIO;
	}

	LOG_DBG("Presc: %d, TS1: %d, TS2: %d",
		timing.prescaler, timing.phase_seg1, timing.phase_seg2);
	LOG_DBG("Sample-point err : %d", ret);

	ret = can_set_timing(dev, &timing);
	if (ret) {
		printk("[%s:%d] err ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}

#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
	/* Disable the reception message buffers as FIFO is used instead */
	sys_write32(0, config->reg_addr + RCAR_CAN_CFDRMNB);

	/* Configure the reception FIFO, with a depth of 128 messages and 64 bytes per message */
	sys_write32(
		/*(FIELD_PREP(RCAR_CAN_CFDRFCC0_RFDC_MASK, RCAR_CAN_CFDRFCC0_RFDC_DEPTH_128) << RCAR_CAN_CFDRFCC0_RFDC_SHIFT) |
		(FIELD_PREP(RCAR_CAN_CFDRFCC0_RFPLS_MASK, RCAR_CAN_CFDRFCC0_RFPLS_SIZE_64) << RCAR_CAN_CFDRFCC0_RFPLS_SHIFT) |
		(FIELD_PREP(RCAR_CAN_CFDRFCC0_RFE_MASK, RCAR_CAN_CFDRFCC0_RFE_ENABLE) << RCAR_CAN_CFDRFCC0_RFE_SHIFT),*/
		(RCAR_CAN_CFDRFCC0_RFDC_DEPTH_128 << RCAR_CAN_CFDRFCC0_RFDC_SHIFT) |
		(RCAR_CAN_CFDRFCC0_RFPLS_SIZE_64 << RCAR_CAN_CFDRFCC0_RFPLS_SHIFT)/* |*/
		/*(RCAR_CAN_CFDRFCC0_RFE_ENABLE << RCAR_CAN_CFDRFCC0_RFE_SHIFT)*/,
		config->reg_addr + RCAR_CAN_CFDRFCC0);

	/* Configure a common FIFO for transmission, with a depth of 128 messages and 64 bytes per message */
	sys_write32(
		/*FIELD_PREP(RCAR_CAN_CFDCFCC0_CFDC_MASK, RCAR_CAN_CFDCFCC0_CFDC_DEPTH_128) << RCAR_CAN_CFDCFCC0_CFDC_SHIFT) |
		FIELD_PREP(RCAR_CAN_CFDCFCC0_CFM_MASK, RCAR_CAN_CFDCFCC0_CFM_TX) << RCAR_CAN_CFDCFCC0_CFM_SHIFT) |*/
		(RCAR_CAN_CFDCFCC0_CFDC_DEPTH_128 << RCAR_CAN_CFDCFCC0_CFDC_SHIFT) |
		(RCAR_CAN_CFDCFCC0_CFM_TX << RCAR_CAN_CFDCFCC0_CFM_SHIFT) |
		(RCAR_CAN_CFDCFCC0_CFPLS_SIZE_64 << RCAR_CAN_CFDCFCC0_CFPLS_SHIFT) /*|*/
		/*(RCAR_CAN_CFDCFCC0_CFE_ENABLE << RCAR_CAN_CFDCFCC0_CFE_SHIFT)*/,
		config->reg_addr + RCAR_CAN_CFDCFCC0);
#endif

	ret = can_rcar_set_mode(dev, CAN_MODE_NORMAL);
	if (ret) {
		printk("[%s:%d] err ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}

	// TODO activ interrupts CFDRFCC0

#ifndef CONFIG_SOC_SERIES_RCAR_GEN5
	ctlr = can_rcar_read16(config, RCAR_CAN_CTLR);
	ctlr |= RCAR_CAN_CTLR_IDFM_MIXED;       /* Select mixed ID mode */
	ctlr &= ~RCAR_CAN_CTLR_BOM_ENT;         /* Clear entry to halt automatically at bus-off */
	ctlr |= RCAR_CAN_CTLR_MBM;              /* Select FIFO mailbox mode */
	ctlr |= RCAR_CAN_CTLR_MLM;              /* Overrun mode */
	ctlr &= ~RCAR_CAN_CTLR_SLPM;            /* Clear CAN Sleep mode */
	can_rcar_write16(config, RCAR_CAN_CTLR, ctlr);

	/* Accept all SID and EID */
	sys_write32(0, config->reg_addr + RCAR_CAN_MKR8);
	sys_write32(0, config->reg_addr + RCAR_CAN_MKR9);
	/* In FIFO mailbox mode, write "0" to bits 24 to 31 */
	sys_write32(0, config->reg_addr + RCAR_CAN_MKIVLR0);
	sys_write32(0, config->reg_addr + RCAR_CAN_MKIVLR1);
	/* Accept standard and extended ID frames, but not
	 * remote frame.
	 */
	sys_write32(0, config->reg_addr + RCAR_CAN_FIDCR0);
	sys_write32(RCAR_CAN_FIDCR_IDE,
		    config->reg_addr + RCAR_CAN_FIDCR1);

	/* Enable and configure FIFO mailbox interrupts Rx and Tx */
	sys_write32(RCAR_CAN_MIER1_RXFIE | RCAR_CAN_MIER1_TXFIE,
		    config->reg_addr + RCAR_CAN_MIER1);

	sys_write8(RCAR_CAN_IER_ERSIE | RCAR_CAN_IER_RXFIE | RCAR_CAN_IER_TXFIE,
		   config->reg_addr + RCAR_CAN_IER);

	/* Accumulate error codes */
	sys_write8(RCAR_CAN_ECSR_EDPM, config->reg_addr + RCAR_CAN_ECSR);

	/* Enable interrupts for all type of errors */
	sys_write8(0xFF, config->reg_addr + RCAR_CAN_EIER);
#endif

	//config->init_func(dev);
#endif

	//printk("[%s:%d] fin OK\n", __func__, __LINE__);

	return 0;
}

#define CAN_RCAR_RSCANFD_GLOBAL_INIT(n)								\
	static const struct can_rcar_rscanfd_global_cfg can_rcar_rscanfd_global_cfg_##n = {	\
		.reg = DT_INST_REG_ADDR(n),							\
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)),				\
		.global_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_IDX(n, 0, name),	\
		.module_clk = (clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_IDX(n, 1, name),	\
		.num_enabled_channels = DT_INST_CHILD_NUM_STATUS_OKAY(n)			\
	};											\
	DEVICE_DT_INST_DEFINE(n, can_rcar_rscanfd_global_init,					\
			 NULL,									\
			 NULL,									\
			 &can_rcar_rscanfd_global_cfg_##n,					\
			 POST_KERNEL,								\
			 CONFIG_KERNEL_INIT_PRIORITY_DEVICE,					\
			 NULL);

DT_INST_FOREACH_STATUS_OKAY(CAN_RCAR_RSCANFD_GLOBAL_INIT);

/* Make sure that the initialization order will be respected */
BUILD_ASSERT(CONFIG_CLOCK_CONTROL_INIT_PRIORITY < CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
	     "The clocks must be initialized before the CAN controller.");
BUILD_ASSERT(CONFIG_KERNEL_INIT_PRIORITY_DEVICE < CONFIG_CAN_INIT_PRIORITY,
	     "The CAN controller must be initialized before its channels.");
