/*
 * Copyright (c) 2026 Semy BENADY
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Driver for the Excelitas CaliPile TPiS 1S 1385 infrared thermopile sensor.
 *
 * Implements the sensor API sample_fetch and channel_get entry points. The
 * device is queried through I2C. Object and ambient temperatures are exposed
 * as SI channels (SENSOR_CHAN_DIE_TEMP, SENSOR_CHAN_AMBIENT_TEMP) computed
 * from the raw register values using the calibration constants read from the
 * on-chip EEPROM (see datasheet sections 8.3 and 8.4). The proximity channel
 * (SENSOR_CHAN_PROX) exposes the raw presence and motion counters.
 */

#include "tpi1s1385.h"
#include "tpi1s1385_priv.h"

#include <math.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(TPI1S1385, CONFIG_SENSOR_LOG_LEVEL);

#define DT_DRV_COMPAT excelitas_tpi1s1385

/* General Call reload command (datasheet section 5.5). */
#define TPI1S1385_GENERAL_CALL_RELOAD		0x04

/* Datasheet section 5.5: up to 350 us to load the slave address from EEPROM. */
#define TPI1S1385_EEPROM_RELOAD_DELAY_US	350

/* EEPROM_CONTROL bit that enables EEPROM read access (datasheet section 6.1). */
#define TPI1S1385_EEPROM_CTRL_ENABLE		0x80
#define TPI1S1385_EEPROM_CTRL_DISABLE		0x00

/*
 * Datasheet section 8.4: the object temperature is derived from a non-linear
 * function of the thermopile output. For the default lookup table (LOOKUP=0)
 * the function is f(x) = x^TPI1S1385_LOOKUP_EXP.
 */
#define TPI1S1385_LOOKUP_EXP			3.8

/* 25 degC expressed in Kelvin, used as reference in the calibration formulas. */
#define TPI1S1385_T_REF_KELVIN			298.15
#define TPI1S1385_ZERO_C_KELVIN			273.15

int tpi1s1385_reg_read(const struct device *dev, uint8_t reg, uint8_t *val)
{
	const struct tpi1s1385_config *config = dev->config;

	return i2c_reg_read_byte_dt(&config->i2c, reg, val);
}

int tpi1s1385_reg_write(const struct device *dev, uint8_t reg, uint8_t val)
{
	const struct tpi1s1385_config *config = dev->config;

	return i2c_reg_write_byte_dt(&config->i2c, reg, val);
}

int tpi1s1385_reg_update(const struct device *dev, uint8_t reg,
			 uint8_t mask, uint8_t val)
{
	uint8_t old_val;
	int ret;

	ret = tpi1s1385_reg_read(dev, reg, &old_val);
	if (ret < 0) {
		return ret;
	}

	old_val = (old_val & ~mask) | (val & mask);

	return tpi1s1385_reg_write(dev, reg, old_val);
}

static int tpi1s1385_sample_fetch(const struct device *dev,
				  enum sensor_channel chan)
{
	struct tpi1s1385_data *data = dev->data;
	uint8_t reg01, reg02, reg03, reg04;
	int ret;

	if (chan != SENSOR_CHAN_ALL &&
	    chan != SENSOR_CHAN_AMBIENT_TEMP &&
	    chan != SENSOR_CHAN_DIE_TEMP &&
	    chan != SENSOR_CHAN_PROX) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (chan == SENSOR_CHAN_ALL ||
	    chan == SENSOR_CHAN_DIE_TEMP ||
	    chan == SENSOR_CHAN_AMBIENT_TEMP) {
		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_OBJECT_MSB,
					 &reg01);
		if (ret < 0) {
			LOG_ERR("Failed to read TP_OBJECT MSB (0x01): %d", ret);
			goto unlock;
		}

		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_OBJECT_MID,
					 &reg02);
		if (ret < 0) {
			LOG_ERR("Failed to read TP_OBJECT MID (0x02): %d", ret);
			goto unlock;
		}

		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_OBJECT_LSB,
					 &reg03);
		if (ret < 0) {
			LOG_ERR("Failed to read shared register (0x03): %d",
				ret);
			goto unlock;
		}

		data->tp_object = ((int32_t)reg01 << 9) |
				  ((int32_t)reg02 << 1) |
				  ((int32_t)reg03);

		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_AMBIENT_LSB,
					 &reg04);
		if (ret < 0) {
			LOG_ERR("Failed to read TP_AMBIENT LSB (0x04): %d",
				ret);
			goto unlock;
		}

		data->tp_ambient = (int16_t)(((uint16_t)reg03 << 8) |
					     (uint16_t)reg04);
	}

	if (chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_PROX) {
		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_PRESENCE,
					 &data->tp_presence);
		if (ret < 0) {
			LOG_ERR("Failed to read TP_PRESENCE (0x0F): %d", ret);
			goto unlock;
		}

		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_MOTION,
					 &data->tp_motion);
		if (ret < 0) {
			LOG_ERR("Failed to read TP_MOTION (0x10): %d", ret);
			goto unlock;
		}
	}

	/* Reading INTERRUPT_STATUS clears the latched flags. */
	if (chan == SENSOR_CHAN_ALL) {
		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_INTERRUPT_STATUS,
					 &data->interrupt_status);
		if (ret < 0) {
			LOG_ERR("Failed to read INTERRUPT_STATUS (0x12): %d",
				ret);
			goto unlock;
		}
	}

	ret = 0;
unlock:
	k_mutex_unlock(&data->lock);
	return ret;
}

/*
 * Compute the ambient temperature in Kelvin from the raw TP_AMBIENT counts and
 * the calibration constants stored in EEPROM (datasheet section 8.3):
 *
 *     Tamb_K = 298.15 + (TPambient - PTAT25) * 100 / M
 */
static double tpi1s1385_ambient_kelvin(const struct tpi1s1385_data *data)
{
	return TPI1S1385_T_REF_KELVIN +
	       ((double)data->tp_ambient - (double)data->eeprom.ptat25) *
	       100.0 / (double)data->eeprom.m;
}

/*
 * Compute the object temperature in Kelvin (datasheet section 8.4):
 *
 *     k = (Uout1 - U0) / (f(Tobj1_K) - f(298.15))
 *     f(x) = x^3.8
 *     Tobject_K = F[(TPobject - U0) / k + f(Tamb_K)]
 *     F(x) = x^(1/3.8)
 */
static double tpi1s1385_object_kelvin(const struct tpi1s1385_data *data,
				      double tamb_k)
{
	double u0 = (double)data->eeprom.u0;
	double uout1 = (double)data->eeprom.uout1;
	double tobj1_k = (double)data->eeprom.tobj1 + TPI1S1385_ZERO_C_KELVIN;

	double f_tobj1 = pow(tobj1_k, TPI1S1385_LOOKUP_EXP);
	double f_tref = pow(TPI1S1385_T_REF_KELVIN, TPI1S1385_LOOKUP_EXP);
	double k = (uout1 - u0) / (f_tobj1 - f_tref);

	double f_tamb = pow(tamb_k, TPI1S1385_LOOKUP_EXP);
	double f_tobj = ((double)data->tp_object - u0) / k + f_tamb;

	return pow(f_tobj, 1.0 / TPI1S1385_LOOKUP_EXP);
}

static int tpi1s1385_channel_get(const struct device *dev,
				 enum sensor_channel chan,
				 struct sensor_value *val)
{
	struct tpi1s1385_data *data = dev->data;
	double tamb_c, tobj_c;

	switch (chan) {
	case SENSOR_CHAN_AMBIENT_TEMP:
		tamb_c = tpi1s1385_ambient_kelvin(data) -
			 TPI1S1385_ZERO_C_KELVIN;
		return sensor_value_from_double(val, tamb_c);

	case SENSOR_CHAN_DIE_TEMP:
		tobj_c = tpi1s1385_object_kelvin(
				data, tpi1s1385_ambient_kelvin(data)) -
			 TPI1S1385_ZERO_C_KELVIN;
		return sensor_value_from_double(val, tobj_c);

	case SENSOR_CHAN_PROX:
		val->val1 = (int32_t)data->tp_presence;
		val->val2 = (int32_t)data->tp_motion;
		return 0;

	default:
		return -ENOTSUP;
	}
}

static int tpi1s1385_read_eeprom(const struct device *dev)
{
	struct tpi1s1385_data *data = dev->data;
	uint8_t msb, lsb;
	int ret;

	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_EEPROM_CONTROL,
				  TPI1S1385_EEPROM_CTRL_ENABLE);
	if (ret < 0) {
		LOG_ERR("Failed to enable EEPROM access: %d", ret);
		return ret;
	}

	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_PTAT25_MSB, &msb);
	if (ret < 0) {
		goto done;
	}
	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_PTAT25_LSB, &lsb);
	if (ret < 0) {
		goto done;
	}
	/* PTAT25 is a 15-bit value; bit 7 of the MSB register is reserved. */
	data->eeprom.ptat25 = ((uint16_t)(msb & 0x7F) << 8) | lsb;

	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_M_MSB, &msb);
	if (ret < 0) {
		goto done;
	}
	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_M_LSB, &lsb);
	if (ret < 0) {
		goto done;
	}
	data->eeprom.m = ((uint16_t)msb << 8) | lsb;

	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_U0_MSB, &msb);
	if (ret < 0) {
		goto done;
	}
	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_U0_LSB, &lsb);
	if (ret < 0) {
		goto done;
	}
	data->eeprom.u0 = ((uint16_t)msb << 8) | lsb;

	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_UOUT1_MSB, &msb);
	if (ret < 0) {
		goto done;
	}
	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_UOUT1_LSB, &lsb);
	if (ret < 0) {
		goto done;
	}
	data->eeprom.uout1 = ((uint16_t)msb << 8) | lsb;

	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_TOBJ1,
				 &data->eeprom.tobj1);
	if (ret < 0) {
		goto done;
	}

	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_EEPROM_LOOKUP,
				 &data->eeprom.lookup);

done:
	/* Restore EEPROM_CONTROL to disabled regardless of previous errors. */
	(void)tpi1s1385_reg_write(dev, TPI1S1385_REG_EEPROM_CONTROL,
				  TPI1S1385_EEPROM_CTRL_DISABLE);
	if (ret < 0) {
		LOG_ERR("Failed to read EEPROM calibration: %d", ret);
	}
	return ret;
}

static int tpi1s1385_init(const struct device *dev)
{
	const struct tpi1s1385_config *config = dev->config;
	struct tpi1s1385_data *data = dev->data;
	uint8_t slave_addr;
	uint8_t reload_cmd = TPI1S1385_GENERAL_CALL_RELOAD;
	uint8_t int_status;
	uint8_t slp12;
	uint8_t src_reg;
	int ret;

	k_mutex_init(&data->lock);

	if (!i2c_is_ready_dt(&config->i2c)) {
		LOG_ERR("I2C bus is not ready");
		return -ENODEV;
	}

	/*
	 * Datasheet sections 5.3 and 5.5: after power-up the device only
	 * responds to the General Call Address (0x00). Issue a write to 0x00
	 * with the reload command so the device copies its slave address
	 * from EEPROM into the address register.
	 */
	ret = i2c_write(config->i2c.bus, &reload_cmd, sizeof(reload_cmd), 0x00);
	if (ret < 0) {
		LOG_WRN("General Call failed (ret=%d), device may already be "
			"initialized", ret);
	}

	k_usleep(TPI1S1385_EEPROM_RELOAD_DELAY_US);

	/* Verify the device now answers on its slave address. */
	ret = i2c_reg_read_byte_dt(&config->i2c,
				   TPI1S1385_REG_GENERAL_CALL,
				   &slave_addr);
	if (ret < 0) {
		LOG_ERR("Device did not respond at address 0x%02X: %d",
			config->i2c.addr, ret);
		return -EIO;
	}

	/*
	 * Datasheet section 6.1: control register values are undefined after
	 * power-up and require an initialization procedure.
	 */
	slp12 = ((config->slp2 & 0x0F) << 4) | (config->slp1 & 0x0F);
	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_SLP12, slp12);
	if (ret < 0) {
		LOG_ERR("Failed to write SLP12: %d", ret);
		return ret;
	}

	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_SLP3,
				  config->slp3 & 0x0F);
	if (ret < 0) {
		LOG_ERR("Failed to write SLP3: %d", ret);
		return ret;
	}

	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_PRESENCE_THRESHOLD,
				  config->presence_threshold);
	if (ret < 0) {
		LOG_ERR("Failed to write presence threshold: %d", ret);
		return ret;
	}

	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_MOTION_THRESHOLD,
				  config->motion_threshold);
	if (ret < 0) {
		LOG_ERR("Failed to write motion threshold: %d", ret);
		return ret;
	}

	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_AMB_SHOCK_THRESHOLD,
				  config->amb_shock_threshold);
	if (ret < 0) {
		LOG_ERR("Failed to write ambient shock threshold: %d", ret);
		return ret;
	}

	src_reg = ((config->tpot_direction & 0x01) << 4) |
		  ((config->src_select & 0x03) << 2) |
		  (config->cycle_time & 0x03);
	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_SRC_SELECT, src_reg);
	if (ret < 0) {
		LOG_ERR("Failed to write SRC/cycle/direction: %d", ret);
		return ret;
	}

	/*
	 * Datasheet section 7.5: the over-temperature flag is set after
	 * power-up. Read INTERRUPT_STATUS to clear the latched flags.
	 */
	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_INTERRUPT_STATUS,
				 &int_status);
	if (ret < 0) {
		LOG_ERR("Failed to read INTERRUPT_STATUS: %d", ret);
		return ret;
	}

	/* Datasheet sections 5.8 and 8.1 to 8.4: read the calibration data. */
	ret = tpi1s1385_read_eeprom(dev);
	if (ret < 0) {
		return ret;
	}

	LOG_INF("Excelitas TPiS 1S 1385 initialized at address 0x%02X "
		"(PTAT25=%u, M=%u, LOOKUP=%u)",
		config->i2c.addr, data->eeprom.ptat25, data->eeprom.m,
		data->eeprom.lookup);

#ifdef CONFIG_TPI1S1385_TRIGGER
	ret = tpi1s1385_trigger_init(dev);
	if (ret < 0) {
		LOG_ERR("Failed to initialize trigger: %d", ret);
		return ret;
	}
#endif

	return 0;
}

static DEVICE_API(sensor, tpi1s1385_api) = {
	.sample_fetch = tpi1s1385_sample_fetch,
	.channel_get = tpi1s1385_channel_get,
#ifdef CONFIG_TPI1S1385_TRIGGER
	.trigger_set = tpi1s1385_trigger_set,
	.attr_set = tpi1s1385_attr_set,
#endif
};

#define TPI1S1385_INT_GPIO_INIT(inst)						\
	IF_ENABLED(CONFIG_TPI1S1385_TRIGGER,					\
		(.int_gpio = GPIO_DT_SPEC_INST_GET_OR(inst, int_gpios, {0}),))

#define TPI1S1385_DEFINE(inst)							\
	static struct tpi1s1385_data tpi1s1385_data_##inst;			\
										\
	static const struct tpi1s1385_config tpi1s1385_config_##inst = {	\
		.i2c = I2C_DT_SPEC_INST_GET(inst),				\
		TPI1S1385_INT_GPIO_INIT(inst)					\
		.slp1 = DT_INST_PROP(inst, slp1),				\
		.slp2 = DT_INST_PROP(inst, slp2),				\
		.slp3 = DT_INST_PROP(inst, slp3),				\
		.src_select = DT_INST_PROP(inst, src_select),			\
		.cycle_time = DT_INST_PROP(inst, cycle_time),			\
		.tpot_direction = DT_INST_PROP(inst, tpot_direction),		\
		.presence_threshold = DT_INST_PROP(inst, presence_threshold),	\
		.motion_threshold = DT_INST_PROP(inst, motion_threshold),	\
		.amb_shock_threshold = DT_INST_PROP(inst, amb_shock_threshold),	\
	};									\
										\
	SENSOR_DEVICE_DT_INST_DEFINE(inst,					\
				     tpi1s1385_init,				\
				     NULL,					\
				     &tpi1s1385_data_##inst,			\
				     &tpi1s1385_config_##inst,			\
				     POST_KERNEL,				\
				     CONFIG_SENSOR_INIT_PRIORITY,		\
				     &tpi1s1385_api);

DT_INST_FOREACH_STATUS_OKAY(TPI1S1385_DEFINE)
