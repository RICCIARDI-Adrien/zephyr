/*
 * Copyright (c) 2022 IoT.bzh
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT renesas_pwm_rcar

/*
 * The Gen5 counter has 24 bits, split over PWMCNTA and PWMCNTB, and a running
 * Gen5 channel is updated without being stopped. The Gen5 code is under
 * CONFIG_SOC_SERIES_RCAR_GEN5.
 */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/clock_control/renesas_rcar_generic.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/pwm/rcar_pwm.h>

#include <soc.h>

#define LOG_LEVEL CONFIG_PWM_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(pwm_rcar);

/* PWM Controller capabilities */
#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
#define RCAR_PWM_MAX_CYCLE 0xFFFFFFU
#else
#define RCAR_PWM_MAX_CYCLE   1023U
#define RCAR_PWM_MAX_CHANNEL 6
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */
#define RCAR_PWM_MAX_DIV 24U

#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
/* Only the polarity is checked, RCAR_PWM_CYCLES_SHIFT uses the upper flag bits */
#define RCAR_PWM_CHECKED_FLAGS    PWM_POLARITY_MASK
/* PH = 0 is not allowed and a stopped channel stays high, so 0% is not possible */
#define RCAR_PWM_ZERO_PULSE_ERROR (-ENOTSUP)
#else
/* Gen3 refuses any flag */
#define RCAR_PWM_CHECKED_FLAGS    0xFFFFU
#define RCAR_PWM_ZERO_PULSE_ERROR (-EINVAL)
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */

/* Registers */
#define RCAR_PWM_REG_SHIFT 0x1000
#define RCAR_PWM_CR(channel)                                                                       \
	((uint32_t)((channel * RCAR_PWM_REG_SHIFT)) + 0x00) /* PWM Control Register */
#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
#define RCAR_PWM_CNTA(channel)                                                                     \
	((uint32_t)((channel) * RCAR_PWM_REG_SHIFT) + 0x04) /* PWM Count Register A */
#define RCAR_PWM_CNTB(channel)                                                                     \
	((uint32_t)((channel) * RCAR_PWM_REG_SHIFT) + 0x08) /* PWM Count Register B */
#else
#define RCAR_PWM_CNT(channel)                                                                      \
	((uint32_t)((channel * RCAR_PWM_REG_SHIFT)) + 0x04) /* PWM Count Register */
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */

/* PWMCR (PWM Control Register) */
#define RCAR_PWM_CR_CC_MASK  0x000f0000 /* Clock Control */
#define RCAR_PWM_CR_CC_SHIFT 16
#define RCAR_PWM_CR_CCMD     BIT(15) /* Frequency Division Mode */
#define RCAR_PWM_CR_SYNC     BIT(11)
#define RCAR_PWM_CR_SS	     BIT(4) /* Single Pulse Output */
#define RCAR_PWM_CR_EN	     BIT(0) /* Channel Enable */

/* PWM Diviser is on 5 bits (CC combined with CCMD) */
#define RCAR_PWM_DIVISER_MASK  (RCAR_PWM_CR_CC_MASK | RCAR_PWM_CR_CCMD)
#define RCAR_PWM_DIVISER_SHIFT 15

#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
/* PWMCNTA holds the low 16 bits of CYC and PH, PWMCNTB the high 8 bits */
#define RCAR_PWM_CNT_CYC_SHIFT   16
#define RCAR_PWM_CNTA_LOW_MASK   0xFFFFU
#define RCAR_PWM_CNTB_HIGH_SHIFT 16
#else
/* PWMCNT (PWM Count Register) */
#define RCAR_PWM_CNT_CYC_MASK  0x03ff0000 /* PWM Cycle */
#define RCAR_PWM_CNT_CYC_SHIFT 16
#define RCAR_PWM_CNT_PH_MASK   0x000003ff /* PWM High-Level Period */
#define RCAR_PWM_CNT_PH_SHIFT  0
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */

struct pwm_rcar_cfg {
	uint32_t reg_addr;
	const struct device *clock_dev;
	rcar_generic_clk_t core_clk;
	rcar_generic_clk_t mod_clk;
	const struct pinctrl_dev_config *pcfg;
#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
	uint32_t channels;
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */
};

struct pwm_rcar_data {
	uint32_t clk_rate;
};

static uint32_t pwm_rcar_read(const struct pwm_rcar_cfg *config, uint32_t offs)
{
	return sys_read32(config->reg_addr + offs);
}

static void pwm_rcar_write(const struct pwm_rcar_cfg *config, uint32_t offs, uint32_t value)
{
	sys_write32(value, config->reg_addr + offs);
}

static void pwm_rcar_write_bit(const struct pwm_rcar_cfg *config, uint32_t offs, uint32_t bits,
			       bool value)
{
	uint32_t reg_val = pwm_rcar_read(config, offs);

	if (value) {
		reg_val |= bits;
	} else {
		reg_val &= ~(bits);
	}

	pwm_rcar_write(config, offs, reg_val);
}

/* Gen5 takes the number of channels from reg, Gen3 has a fixed maximum */
static int pwm_rcar_check_channel(const struct pwm_rcar_cfg *config, uint32_t channel)
{
#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
	return (channel < config->channels) ? 0 : -EINVAL;
#else
	ARG_UNUSED(config);

	return (channel <= RCAR_PWM_MAX_CHANNEL) ? 0 : -ENOTSUP;
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */
}

#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
/* Pick the divider, then update the channel without stopping it */
static int pwm_rcar_gen5_update(const struct pwm_rcar_cfg *config, uint32_t channel,
				uint32_t period_cycles, uint32_t pulse_cycles, pwm_flags_t flags)
{
	/* RCAR_PWM_CYCLES_SHIFT(n) counts the period and pulse in units of 2^n cycles */
	uint32_t shift = (flags & RCAR_PWM_CYCLES_SHIFT_MASK) >> RCAR_PWM_CYCLES_SHIFT_POS;
	uint64_t period = (uint64_t)period_cycles << shift;
	uint64_t pulse = (uint64_t)pulse_cycles << shift;
	uint32_t power = 0;
	uint32_t reg_val;
	uint32_t cnta;
	uint32_t cntb;

	/* Smallest divider that fits the period, for the best resolution */
	while (period > RCAR_PWM_MAX_CYCLE) {
		if (power == RCAR_PWM_MAX_DIV) {
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
		pwm_rcar_write_bit(config, RCAR_PWM_CR(channel), RCAR_PWM_CR_EN, false);
		return 0;
	}

	/* The pulse got lost in the division */
	if (pulse_cycles == 0U) {
		return -ENOTSUP;
	}

	LOG_DBG("channel %u: period %u, pulse %u, divider 2^%u", channel, period_cycles,
		pulse_cycles, power);

	cnta = ((period_cycles & RCAR_PWM_CNTA_LOW_MASK) << RCAR_PWM_CNT_CYC_SHIFT) |
	       (pulse_cycles & RCAR_PWM_CNTA_LOW_MASK);
	cntb = ((period_cycles >> RCAR_PWM_CNTB_HIGH_SHIFT) << RCAR_PWM_CNT_CYC_SHIFT) |
	       (pulse_cycles >> RCAR_PWM_CNTB_HIGH_SHIFT);

	/*
	 * Sequence from the manual: set SYNC, write the counts, then write PWMCR
	 * with the divider. All of it applies at the end of the running period.
	 */
	reg_val = pwm_rcar_read(config, RCAR_PWM_CR(channel));
	reg_val |= RCAR_PWM_CR_SYNC;
	pwm_rcar_write(config, RCAR_PWM_CR(channel), reg_val);

	pwm_rcar_write(config, RCAR_PWM_CNTA(channel), cnta);
	pwm_rcar_write(config, RCAR_PWM_CNTB(channel), cntb);

	/* SYNC stays set, otherwise an update could spread over several periods */
	reg_val &= ~RCAR_PWM_DIVISER_MASK;
	reg_val |= (power << RCAR_PWM_DIVISER_SHIFT) | RCAR_PWM_CR_EN;
	pwm_rcar_write(config, RCAR_PWM_CR(channel), reg_val);

	return 0;
}
#else
static int pwm_rcar_update_clk(const struct pwm_rcar_cfg *config, uint32_t channel,
			       uint32_t *period_cycles, uint32_t *pulse_cycles)
{
	uint32_t reg_val, power, diviser;

	power = pwm_rcar_read(config, RCAR_PWM_CR(channel)) & RCAR_PWM_DIVISER_MASK;
	power = power >> RCAR_PWM_DIVISER_SHIFT;
	diviser = 1 << power;

	LOG_DBG("Found old diviser : 2^%d=%d", power, diviser);

	/* Looking for the best possible clock diviser */
	if (*period_cycles > RCAR_PWM_MAX_CYCLE) {
		/* Reducing clock speed */
		while (*period_cycles > RCAR_PWM_MAX_CYCLE) {
			diviser *= 2;
			*period_cycles /= 2;
			*pulse_cycles /= 2;
			power++;
			if (power > RCAR_PWM_MAX_DIV) {
				return -ENOTSUP;
			}
		}
	} else {
		/* Increasing clock speed */
		while (*period_cycles < (RCAR_PWM_MAX_CYCLE / 2)) {
			if (power == 0) {
				return -ENOTSUP;
			}
			diviser /= 2;
			*period_cycles *= 2;
			*pulse_cycles *= 2;
			power--;
		}
	}
	LOG_DBG("Found new diviser : 2^%d=%d\n", power, diviser);

	/* Set new clock Diviser */
	reg_val = pwm_rcar_read(config, RCAR_PWM_CR(channel));
	reg_val &= ~RCAR_PWM_DIVISER_MASK;
	reg_val |= (power << RCAR_PWM_DIVISER_SHIFT);
	pwm_rcar_write(config, RCAR_PWM_CR(channel), reg_val);

	return 0;
}

static int pwm_rcar_gen3_update(const struct pwm_rcar_cfg *config, uint32_t channel,
				uint32_t period_cycles, uint32_t pulse_cycles)
{
	uint32_t reg_val;
	int ret = 0;

	LOG_DBG("base_reg=0x%x, pulse_cycles=%d, period_cycles=%d,"
		" duty_cycle=%d",
		config->reg_addr, pulse_cycles, period_cycles,
		(pulse_cycles * 100U / period_cycles));

	/* Disable PWM */
	pwm_rcar_write_bit(config, RCAR_PWM_CR(channel), RCAR_PWM_CR_EN, false);

	/* Set continuous mode */
	pwm_rcar_write_bit(config, RCAR_PWM_CR(channel), RCAR_PWM_CR_SS, false);

	/* Enable SYNC mode */
	pwm_rcar_write_bit(config, RCAR_PWM_CR(channel), RCAR_PWM_CR_SYNC, true);

	/*
	 * Set clock counter according to the requested period_cycles
	 * if period_cycles is less than half of the counter, then the
	 * clock diviser could be updated as the diviser is a modulo 2.
	 */
	if (period_cycles > RCAR_PWM_MAX_CYCLE || period_cycles < (RCAR_PWM_MAX_CYCLE / 2)) {
		LOG_DBG("Adapting frequency diviser...");
		ret = pwm_rcar_update_clk(config, channel, &period_cycles, &pulse_cycles);
		if (ret != 0) {
			return ret;
		}
	}

	/* Set total period cycle */
	reg_val = pwm_rcar_read(config, RCAR_PWM_CNT(channel));
	reg_val &= ~(RCAR_PWM_CNT_CYC_MASK);
	reg_val |= (period_cycles << RCAR_PWM_CNT_CYC_SHIFT);
	pwm_rcar_write(config, RCAR_PWM_CNT(channel), reg_val);

	/* Set high level period cycle */
	reg_val = pwm_rcar_read(config, RCAR_PWM_CNT(channel));
	reg_val &= ~(RCAR_PWM_CNT_PH_MASK);
	reg_val |= (pulse_cycles << RCAR_PWM_CNT_PH_SHIFT);
	pwm_rcar_write(config, RCAR_PWM_CNT(channel), reg_val);

	/* Enable PWM */
	pwm_rcar_write_bit(config, RCAR_PWM_CR(channel), RCAR_PWM_CR_EN, true);

	return ret;
}
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */

static int pwm_rcar_set_cycles(const struct device *dev, uint32_t channel, uint32_t period_cycles,
			       uint32_t pulse_cycles, pwm_flags_t flags)
{
	const struct pwm_rcar_cfg *config = dev->config;
	int ret;

	ret = pwm_rcar_check_channel(config, channel);
	if (ret < 0) {
		return ret;
	}

	if ((flags & RCAR_PWM_CHECKED_FLAGS) != PWM_POLARITY_NORMAL) {
		return -ENOTSUP;
	}

	/* Prohibited values */
	if (period_cycles == 0U || pulse_cycles > period_cycles) {
		return -EINVAL;
	}

	if (pulse_cycles == 0U) {
		return RCAR_PWM_ZERO_PULSE_ERROR;
	}

#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
	return pwm_rcar_gen5_update(config, channel, period_cycles, pulse_cycles, flags);
#else
	return pwm_rcar_gen3_update(config, channel, period_cycles, pulse_cycles);
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */
}

static int pwm_rcar_get_cycles_per_sec(const struct device *dev, uint32_t channel, uint64_t *cycles)
{
	const struct pwm_rcar_cfg *config = dev->config;
	struct pwm_rcar_data *data = dev->data;
#ifndef CONFIG_SOC_SERIES_RCAR_GEN5
	uint32_t diviser;
#endif /* !CONFIG_SOC_SERIES_RCAR_GEN5 */
	int ret;

	ret = pwm_rcar_check_channel(config, channel);
	if (ret < 0) {
		return ret;
	}

#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
	/* Undivided rate, set_cycles() picks the divider on each call */
	*cycles = data->clk_rate;
#else
	diviser = pwm_rcar_read(config, RCAR_PWM_CR(channel)) & RCAR_PWM_DIVISER_MASK;
	diviser = diviser >> RCAR_PWM_DIVISER_SHIFT;
	*cycles = data->clk_rate >> diviser;

	LOG_DBG("Actual division: %d and Frequency: %d Hz", diviser, (uint32_t)*cycles);
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */

	return 0;
}

static int pwm_rcar_init(const struct device *dev)
{
	const struct pwm_rcar_cfg *config = dev->config;
	struct pwm_rcar_data *data = dev->data;
	int ret;

	/* Configure dt provided device signals when available */
	ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
	if (ret < 0) {
		return ret;
	}

#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
	if (!device_is_ready(config->clock_dev)) {
		return -ENODEV;
	}
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */

	ret = clock_control_on(config->clock_dev, RCAR_CLOCK_SUBSYS(config->mod_clk));
	if (ret < 0) {
		return ret;
	}

	ret = clock_control_get_rate(config->clock_dev, RCAR_CLOCK_SUBSYS(config->core_clk),
				     &data->clk_rate);

	if (ret < 0) {
		return ret;
	}

#ifdef CONFIG_SOC_SERIES_RCAR_GEN5
	LOG_DBG("counter clock: %u Hz", data->clk_rate);

	/* Channels are left as they are, as another core may already use some */
	for (uint32_t channel = 0; channel < config->channels; channel++) {
		uint32_t cr = pwm_rcar_read(config, RCAR_PWM_CR(channel));

		if ((cr & RCAR_PWM_CR_EN) != 0U) {
			LOG_DBG("channel %u already running at boot (PWMCR 0x%08x)", channel, cr);
		}
	}
#endif /* CONFIG_SOC_SERIES_RCAR_GEN5 */

	return 0;
}

static DEVICE_API(pwm, pwm_rcar_driver_api) = {
	.set_cycles = pwm_rcar_set_cycles,
	.get_cycles_per_sec = pwm_rcar_get_cycles_per_sec,
};

/* Device Instantiation */
#define PWM_DEVICE_RCAR_INIT(n)                                                                    \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
	static const struct pwm_rcar_cfg pwm_rcar_cfg_##n = {                                      \
		.reg_addr = DT_INST_REG_ADDR(n),                                                   \
		.pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                         \
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)),                                \
		.mod_clk = RCAR_DT_INST_CLOCKS_CELL_BY_IDX(n, 0),                                  \
		.core_clk = RCAR_DT_INST_CLOCKS_CELL_BY_IDX(n, 1),                                 \
		IF_ENABLED(CONFIG_SOC_SERIES_RCAR_GEN5,                                            \
			   (.channels = DT_INST_REG_SIZE(n) / RCAR_PWM_REG_SHIFT,))                \
	};                                                                                         \
	static struct pwm_rcar_data pwm_rcar_data_##n;                                             \
	DEVICE_DT_INST_DEFINE(n, pwm_rcar_init, NULL, &pwm_rcar_data_##n, &pwm_rcar_cfg_##n,       \
			      POST_KERNEL, CONFIG_PWM_INIT_PRIORITY,                               \
			      &pwm_rcar_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PWM_DEVICE_RCAR_INIT)
