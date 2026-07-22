/*
 * Copyright (c) 2026 BayLibre <https://baylibre.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Sensor API driver for the Excelitas CaliPile TPiS 1S 1385 infrared
 * thermopile. The device is accessed over I2C. It exposes the object and
 * ambient temperatures as SENSOR_CHAN_DIE_TEMP and SENSOR_CHAN_AMBIENT_TEMP
 * and the raw presence and motion counters as SENSOR_CHAN_PROX. The
 * calibration constants required to convert the raw counts into Celsius are
 * read once from the on chip EEPROM at initialization.
 */

#include "tpi1s1385.h"

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

/* Command byte sent through the I2C General Call to reload the slave address. */
#define TPI1S1385_GENERAL_CALL_RELOAD		0x04

/* Time the device needs to copy its slave address from EEPROM into the address register. */
#define TPI1S1385_EEPROM_RELOAD_DELAY_US	350

/* Values written to EEPROM_CONTROL to enable or disable EEPROM read access. */
#define TPI1S1385_EEPROM_CTRL_ENABLE		0x80
#define TPI1S1385_EEPROM_CTRL_DISABLE		0x00

/* Exponent of the lookup function used to compute the object temperature. */
#define TPI1S1385_LOOKUP_EXP			3.8

/* Reference temperatures used by the calibration formulas. */
#define TPI1S1385_T_REF_KELVIN			298.15
#define TPI1S1385_ZERO_C_KELVIN			273.15

/* Same reference temperatures expressed as integers in milli Kelvin. */
#define TPI1S1385_T_REF_MILLIKELVIN		298150
#define TPI1S1385_ZERO_C_MILLIKELVIN		273150

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
			LOG_ERR("Failed to read TP_OBJECT MSB (0x%02X): %d",
				TPI1S1385_REG_TP_OBJECT_MSB, ret);
			goto unlock;
		}

		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_OBJECT_MID,
					 &reg02);
		if (ret < 0) {
			LOG_ERR("Failed to read TP_OBJECT MID (0x%02X): %d",
				TPI1S1385_REG_TP_OBJECT_MID, ret);
			goto unlock;
		}

		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_OBJECT_LSB,
					 &reg03);
		if (ret < 0) {
			LOG_ERR("Failed to read shared register (0x%02X): %d",
				TPI1S1385_REG_TP_OBJECT_LSB, ret);
			goto unlock;
		}

		data->tp_object =
			(int32_t)((reg01 << TPI1S1385_TP_OBJECT_MSB_SHIFT) |
				  (reg02 << TPI1S1385_TP_OBJECT_MID_SHIFT) |
				  ((reg03 & TPI1S1385_TP_OBJECT_LSB_MASK) >>
					TPI1S1385_TP_OBJECT_LSB_SHIFT));

		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_AMBIENT_LSB,
					 &reg04);
		if (ret < 0) {
			LOG_ERR("Failed to read TP_AMBIENT LSB (0x%02X): %d",
				TPI1S1385_REG_TP_AMBIENT_LSB, ret);
			goto unlock;
		}

		data->tp_ambient =
			(int16_t)(((reg03 & TPI1S1385_TP_AMBIENT_MSB_MASK) <<
					TPI1S1385_TP_AMBIENT_MSB_SHIFT) |
				  reg04);
	}

	if (chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_PROX) {
		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_PRESENCE,
					 &data->tp_presence);
		if (ret < 0) {
			LOG_ERR("Failed to read TP_PRESENCE (0x%02X): %d",
				TPI1S1385_REG_TP_PRESENCE, ret);
			goto unlock;
		}

		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_TP_MOTION,
					 &data->tp_motion);
		if (ret < 0) {
			LOG_ERR("Failed to read TP_MOTION (0x%02X): %d",
				TPI1S1385_REG_TP_MOTION, ret);
			goto unlock;
		}
	}

	/* Reading INTERRUPT_STATUS clears the latched flags. */
	if (chan == SENSOR_CHAN_ALL) {
		ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_INTERRUPT_STATUS,
					 &data->interrupt_status);
		if (ret < 0) {
			LOG_ERR("Failed to read INTERRUPT_STATUS (0x%02X): %d",
				TPI1S1385_REG_INTERRUPT_STATUS, ret);
			goto unlock;
		}
	}

	ret = 0;
unlock:
	k_mutex_unlock(&data->lock);
	return ret;
}

/*
 * Ambient temperature in milli Kelvin, computed from the raw counts and the
 * PTAT25 and M calibration constants. The intermediate product is widened
 * to 64 bits because the numerator can exceed the int32 range.
 */
static int32_t tpi1s1385_ambient_millikelvin(const struct tpi1s1385_data *data)
{
	int32_t delta = (int32_t)data->tp_ambient - (int32_t)data->eeprom.ptat25;

	return TPI1S1385_T_REF_MILLIKELVIN +
	       (int32_t)(((int64_t)delta * 100000) / data->eeprom.m);
}

/*
 * Object temperature in Kelvin, computed from the raw counts, the ambient
 * temperature and the U0, Uout1 and Tobj1 calibration constants.
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
	int32_t tamb_mc;
	double tobj_c;

	switch (chan) {
	case SENSOR_CHAN_AMBIENT_TEMP:
		tamb_mc = tpi1s1385_ambient_millikelvin(data) -
			  TPI1S1385_ZERO_C_MILLIKELVIN;
		val->val1 = tamb_mc / 1000;
		val->val2 = (tamb_mc % 1000) * 1000;
		return 0;

	case SENSOR_CHAN_DIE_TEMP:
		tobj_c = tpi1s1385_object_kelvin(
				data,
				tpi1s1385_ambient_millikelvin(data) / 1000.0) -
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
	data->eeprom.ptat25 =
		((uint16_t)(msb & TPI1S1385_EEPROM_PTAT25_MSB_MASK) << 8) | lsb;

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
	/* EEPROM_CONTROL is always restored to the disabled value on exit. */
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
	 * After power up the device only replies to the General Call address.
	 * Sending the reload command tells it to copy its own slave address
	 * from EEPROM into the address register.
	 */
	ret = i2c_write(config->i2c.bus, &reload_cmd, sizeof(reload_cmd), 0x00);
	if (ret < 0) {
		LOG_WRN("General Call failed (ret=%d), device may already be initialized", ret);
	}

	k_usleep(TPI1S1385_EEPROM_RELOAD_DELAY_US);

	/* Read back a register to verify the device now answers at its slave address. */
	ret = i2c_reg_read_byte_dt(&config->i2c,
				   TPI1S1385_REG_GENERAL_CALL,
				   &slave_addr);
	if (ret < 0) {
		LOG_ERR("Device did not respond at address 0x%02X: %d",
			config->i2c.addr, ret);
		return -EIO;
	}

	/*
	 * Control register values are undefined after power up and must be
	 * programmed before the sensor can be used.
	 */
	slp12 = ((config->slp2 & TPI1S1385_SLP_FIELD_MASK) << TPI1S1385_SLP2_SHIFT) |
		(config->slp1 & TPI1S1385_SLP_FIELD_MASK);
	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_SLP12, slp12);
	if (ret < 0) {
		LOG_ERR("Failed to write SLP12: %d", ret);
		return ret;
	}

	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_SLP3,
				  config->slp3 & TPI1S1385_SLP_FIELD_MASK);
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

	src_reg = ((config->tpot_direction & TPI1S1385_TPOT_DIRECTION_MASK) <<
			TPI1S1385_TPOT_DIRECTION_SHIFT) |
		  ((config->src_select & TPI1S1385_SRC_SELECT_MASK) <<
			TPI1S1385_SRC_SELECT_SHIFT) |
		  (config->cycle_time & TPI1S1385_CYCLE_TIME_MASK);
	ret = tpi1s1385_reg_write(dev, TPI1S1385_REG_SRC_SELECT, src_reg);
	if (ret < 0) {
		LOG_ERR("Failed to write SRC/cycle/direction: %d", ret);
		return ret;
	}

	/*
	 * The over temperature flag is asserted at power up. Reading
	 * INTERRUPT_STATUS here clears any latched flag before the
	 * application starts using the trigger interface.
	 */
	ret = tpi1s1385_reg_read(dev, TPI1S1385_REG_INTERRUPT_STATUS,
				 &int_status);
	if (ret < 0) {
		LOG_ERR("Failed to read INTERRUPT_STATUS: %d", ret);
		return ret;
	}

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
