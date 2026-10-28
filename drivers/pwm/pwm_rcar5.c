/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * PWM driver for the Renesas R-Car Gen5 r8a78000. The block is close to the
 * Gen3 one handled by pwm_rcar.c, but its counter has 24 bits split over two
 * registers, which changes most of the code.
 */

#define DT_DRV_COMPAT renesas_pwm_rcar5

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/clock_control/renesas_rcar_generic.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/pwm/rcar_pwm.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/util.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pwm_rcar5, CONFIG_PWM_LOG_LEVEL);

/* The counter has 24 bits, and its clock can be divided by up to 2^24 */
#define RCAR5_PWM_MAX_CYCLE 0xFFFFFFU
#define RCAR5_PWM_MAX_DIV   24U

/* Each channel has its own registers, 0x1000 after those of the previous one */
#define RCAR5_PWM_REG_SHIFT 0x1000
#define RCAR5_PWM_CR(channel)                                                                      \
	((uint32_t)((channel) * RCAR5_PWM_REG_SHIFT) + 0x00) /* PWM Control Register */
#define RCAR5_PWM_CNTA(channel)                                                                    \
	((uint32_t)((channel) * RCAR5_PWM_REG_SHIFT) + 0x04) /* PWM Count Register A */
#define RCAR5_PWM_CNTB(channel)                                                                    \
	((uint32_t)((channel) * RCAR5_PWM_REG_SHIFT) + 0x08) /* PWM Count Register B */

/* PWMCR fields */
#define RCAR5_PWM_CR_CC_MASK GENMASK(19, 16) /* Clock Control */
#define RCAR5_PWM_CR_CCMD    BIT(15)         /* Frequency Division Mode */
#define RCAR5_PWM_CR_SYNC    BIT(11)         /* Synchronous Update Mode */
#define RCAR5_PWM_CR_EN      BIT(0)          /* Channel Enable */

/* CC and CCMD together hold n, for a clock divided by 2^n */
#define RCAR5_PWM_DIVISER_MASK  (RCAR5_PWM_CR_CC_MASK | RCAR5_PWM_CR_CCMD)
#define RCAR5_PWM_DIVISER_SHIFT 15

/* PWMCNTA holds the low 16 bits of CYC and PH, PWMCNTB the high 8 bits */
#define RCAR5_PWM_CNT_CYC_SHIFT   16
#define RCAR5_PWM_CNTA_LOW_MASK   0xFFFFU
#define RCAR5_PWM_CNTB_HIGH_SHIFT 16

struct pwm_rcar5_config {
	mm_reg_t base;
	uint32_t channels;
	const struct device *clock_dev;
	rcar_generic_clk_t mod_clk;  /* Turns the block on */
	rcar_generic_clk_t core_clk; /* Drives the counter */
	const struct pinctrl_dev_config *pcfg;
};

struct pwm_rcar5_data {
	uint32_t clk_rate; /* Counter clock rate, read once at init */
};

static uint32_t pwm_rcar5_read(const struct pwm_rcar5_config *config, uint32_t offs)
{
	return sys_read32(config->base + offs);
}

static void pwm_rcar5_write(const struct pwm_rcar5_config *config, uint32_t offs, uint32_t value)
{
	sys_write32(value, config->base + offs);
}

/* Pick the divider, then update the channel without stopping it */
static int pwm_rcar5_set_cycles(const struct device *dev, uint32_t channel, uint32_t period_cycles,
				uint32_t pulse_cycles, pwm_flags_t flags)
{
	const struct pwm_rcar5_config *config = dev->config;
	/* RCAR_PWM_CYCLES_SHIFT(n) counts the period and pulse in units of 2^n cycles */
	uint32_t shift = (flags & RCAR_PWM_CYCLES_SHIFT_MASK) >> RCAR_PWM_CYCLES_SHIFT_POS;
	uint64_t period = (uint64_t)period_cycles << shift;
	uint64_t pulse = (uint64_t)pulse_cycles << shift;
	uint32_t power = 0;
	uint32_t cnta;
	uint32_t cntb;
	uint32_t cr;

	if (channel >= config->channels) {
		return -EINVAL;
	}

	if ((flags & PWM_POLARITY_MASK) != PWM_POLARITY_NORMAL) {
		return -ENOTSUP;
	}

	if (period_cycles == 0U || pulse_cycles > period_cycles) {
		return -EINVAL;
	}

	/* PH = 0 is not allowed and a stopped channel stays high, so 0% is not possible */
	if (pulse_cycles == 0U) {
		return -ENOTSUP;
	}

	/* Smallest divider that fits the period, for the best resolution */
	while (period > RCAR5_PWM_MAX_CYCLE) {
		if (power == RCAR5_PWM_MAX_DIV) {
			return -ENOTSUP;
		}
		period /= 2U;
		pulse /= 2U;
		power++;
	}
	period_cycles = (uint32_t)period;
	pulse_cycles = (uint32_t)pulse;

	/*
	 * A stopped channel stays high, so 100% just stops it, at the end of the
	 * running period. This is checked after the division, which can make the
	 * pulse equal to the period.
	 */
	if (pulse_cycles == period_cycles) {
		cr = pwm_rcar5_read(config, RCAR5_PWM_CR(channel));
		cr &= ~RCAR5_PWM_CR_EN;
		pwm_rcar5_write(config, RCAR5_PWM_CR(channel), cr);
		return 0;
	}

	/* The pulse got lost in the division */
	if (pulse_cycles == 0U) {
		return -ENOTSUP;
	}

	LOG_DBG("channel %u: period %u, pulse %u, divider 2^%u", channel, period_cycles,
		pulse_cycles, power);

	cnta = ((period_cycles & RCAR5_PWM_CNTA_LOW_MASK) << RCAR5_PWM_CNT_CYC_SHIFT) |
	       (pulse_cycles & RCAR5_PWM_CNTA_LOW_MASK);
	cntb = ((period_cycles >> RCAR5_PWM_CNTB_HIGH_SHIFT) << RCAR5_PWM_CNT_CYC_SHIFT) |
	       (pulse_cycles >> RCAR5_PWM_CNTB_HIGH_SHIFT);

	/*
	 * Sequence from the manual: set SYNC, write the counts, then write PWMCR
	 * with the divider. All of it applies at the end of the running period.
	 */
	cr = pwm_rcar5_read(config, RCAR5_PWM_CR(channel));
	cr |= RCAR5_PWM_CR_SYNC;
	pwm_rcar5_write(config, RCAR5_PWM_CR(channel), cr);

	pwm_rcar5_write(config, RCAR5_PWM_CNTA(channel), cnta);
	pwm_rcar5_write(config, RCAR5_PWM_CNTB(channel), cntb);

	/* SYNC stays set, otherwise an update could spread over several periods */
	cr &= ~RCAR5_PWM_DIVISER_MASK;
	cr |= (power << RCAR5_PWM_DIVISER_SHIFT) | RCAR5_PWM_CR_EN;
	pwm_rcar5_write(config, RCAR5_PWM_CR(channel), cr);

	return 0;
}

/* Undivided rate, set_cycles() picks the divider on each call */
static int pwm_rcar5_get_cycles_per_sec(const struct device *dev, uint32_t channel,
					uint64_t *cycles)
{
	const struct pwm_rcar5_config *config = dev->config;
	struct pwm_rcar5_data *data = dev->data;

	if (channel >= config->channels) {
		return -EINVAL;
	}

	*cycles = data->clk_rate;

	return 0;
}

/* Channels are left as they are, as another core may already use some */
static int pwm_rcar5_init(const struct device *dev)
{
	const struct pwm_rcar5_config *config = dev->config;
	struct pwm_rcar5_data *data = dev->data;
	int ret;

	ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

	if (!device_is_ready(config->clock_dev)) {
		return -ENODEV;
	}

	ret = clock_control_on(config->clock_dev, RCAR_CLOCK_SUBSYS(config->mod_clk));
	if (ret < 0) {
		return ret;
	}

	ret = clock_control_get_rate(config->clock_dev, RCAR_CLOCK_SUBSYS(config->core_clk),
				     &data->clk_rate);
	if (ret < 0) {
		return ret;
	}

	LOG_DBG("counter clock: %u Hz", data->clk_rate);

	for (uint32_t channel = 0; channel < config->channels; channel++) {
		uint32_t cr = pwm_rcar5_read(config, RCAR5_PWM_CR(channel));

		if ((cr & RCAR5_PWM_CR_EN) != 0U) {
			LOG_DBG("channel %u already running at boot (PWMCR 0x%08x)", channel, cr);
		}
	}

	return 0;
}

static DEVICE_API(pwm, pwm_rcar5_driver_api) = {
	.set_cycles = pwm_rcar5_set_cycles,
	.get_cycles_per_sec = pwm_rcar5_get_cycles_per_sec,
};

/* One instance per controller, with as many channels as its register area holds */
#define PWM_RCAR5_INIT(n)                                                                          \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
	static const struct pwm_rcar5_config pwm_rcar5_config_##n = {                              \
		.base = DT_INST_REG_ADDR(n),                                                       \
		.channels = DT_INST_REG_SIZE(n) / RCAR5_PWM_REG_SHIFT,                             \
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)),                                \
		.mod_clk = RCAR_DT_INST_CLOCKS_CELL_BY_IDX(n, 0),                                  \
		.core_clk = RCAR_DT_INST_CLOCKS_CELL_BY_IDX(n, 1),                                 \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                         \
	};                                                                                         \
	static struct pwm_rcar5_data pwm_rcar5_data_##n;                                           \
	DEVICE_DT_INST_DEFINE(n, pwm_rcar5_init, NULL, &pwm_rcar5_data_##n, &pwm_rcar5_config_##n, \
			      POST_KERNEL, CONFIG_PWM_INIT_PRIORITY, &pwm_rcar5_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_RCAR5_INIT)
