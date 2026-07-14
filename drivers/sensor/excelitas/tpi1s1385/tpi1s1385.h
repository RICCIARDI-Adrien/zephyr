/*
 * Copyright (c) 2026 Semy BENADY
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_EXCELITAS_TPI1S1385_TPI1S1385_H_
#define ZEPHYR_DRIVERS_SENSOR_EXCELITAS_TPI1S1385_TPI1S1385_H_

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/sensor.h>

#ifdef CONFIG_TPI1S1385_TRIGGER
#include <zephyr/drivers/gpio.h>
#endif

/* Register map (see datasheet section 6.1) */
#define TPI1S1385_REG_GENERAL_CALL		0x00

#define TPI1S1385_REG_TP_OBJECT_MSB		0x01
#define TPI1S1385_REG_TP_OBJECT_MID		0x02
/*
 * Reg 0x03 is shared: it holds the least significant bit of the 17-bit
 * TP_OBJECT value and the most significant byte of the 16-bit TP_AMBIENT
 * value (see datasheet section 6.1).
 */
#define TPI1S1385_REG_TP_OBJECT_LSB		0x03
#define TPI1S1385_REG_TP_AMBIENT_MSB		0x03
#define TPI1S1385_REG_TP_AMBIENT_LSB		0x04

#define TPI1S1385_REG_TP_OBJ_LP1_MSB		0x05
#define TPI1S1385_REG_TP_OBJ_LP1_MID		0x06
/* Reg 0x07 is shared between LP1 LSB and LP2 MSB. */
#define TPI1S1385_REG_TP_OBJ_LP1_LSB		0x07
#define TPI1S1385_REG_TP_OBJ_LP2_MSB		0x07
#define TPI1S1385_REG_TP_OBJ_LP2_MID		0x08
#define TPI1S1385_REG_TP_OBJ_LP2_LSB		0x09

#define TPI1S1385_REG_TP_AMB_LP3_MSB		0x0A
#define TPI1S1385_REG_TP_AMB_LP3_LSB		0x0B

#define TPI1S1385_REG_TP_OBJ_LP2F_MSB		0x0C
#define TPI1S1385_REG_TP_OBJ_LP2F_MID		0x0D
#define TPI1S1385_REG_TP_OBJ_LP2F_LSB		0x0E

#define TPI1S1385_REG_TP_PRESENCE		0x0F
#define TPI1S1385_REG_TP_MOTION			0x10
#define TPI1S1385_REG_TP_AMB_SHOCK		0x11

/* Read clears the flags. */
#define TPI1S1385_REG_INTERRUPT_STATUS		0x12
#define TPI1S1385_REG_STATUS			0x13

/* Low-pass filter time constants (datasheet section 6.2). */
#define TPI1S1385_REG_SLP12			0x14
#define TPI1S1385_REG_SLP3			0x15

#define TPI1S1385_REG_PRESENCE_THRESHOLD	0x16
#define TPI1S1385_REG_MOTION_THRESHOLD		0x17
#define TPI1S1385_REG_AMB_SHOCK_THRESHOLD	0x18

#define TPI1S1385_REG_INTERRUPT_MASK		0x19
#define TPI1S1385_REG_SRC_SELECT		0x1A

#define TPI1S1385_REG_TIMER_INTERRUPT		0x1B

#define TPI1S1385_REG_TPOT_THRESHOLD_MSB	0x1C
#define TPI1S1385_REG_TPOT_THRESHOLD_LSB	0x1D

#define TPI1S1385_REG_EEPROM_CONTROL		0x1F
#define TPI1S1385_REG_EEPROM_PROTOCOL		0x20
#define TPI1S1385_REG_EEPROM_CHECKSUM		0x21

/* Calibration constants stored in EEPROM (datasheet section 8.1). */
#define TPI1S1385_REG_EEPROM_LOOKUP		41
#define TPI1S1385_REG_EEPROM_PTAT25_MSB		42
#define TPI1S1385_REG_EEPROM_PTAT25_LSB		43
#define TPI1S1385_REG_EEPROM_M_MSB		44
#define TPI1S1385_REG_EEPROM_M_LSB		45
#define TPI1S1385_REG_EEPROM_U0_MSB		46
#define TPI1S1385_REG_EEPROM_U0_LSB		47
#define TPI1S1385_REG_EEPROM_UOUT1_MSB		48
#define TPI1S1385_REG_EEPROM_UOUT1_LSB		49
#define TPI1S1385_REG_EEPROM_TOBJ1		50

#define TPI1S1385_REG_SLAVE_ADDRESS		0x3F

/* Interrupt status/mask bits (register 0x12 and 0x19). */
#define TPI1S1385_INT_TPOT			BIT(4)	/* Over-temperature */
#define TPI1S1385_INT_TP_PRESENCE		BIT(3)	/* Presence */
#define TPI1S1385_INT_TP_MOTION			BIT(2)	/* Motion */
#define TPI1S1385_INT_TP_AMB_SHOCK		BIT(1)	/* Ambient shock */
#define TPI1S1385_INT_TIMER			BIT(0)	/* Timer */

/* Default I2C slave address after EEPROM reload (datasheet section 5.5). */
#define TPI1S1385_DEFAULT_I2C_ADDR		0x0C

int tpi1s1385_reg_read(const struct device *dev, uint8_t reg, uint8_t *val);
int tpi1s1385_reg_write(const struct device *dev, uint8_t reg, uint8_t val);
int tpi1s1385_reg_update(const struct device *dev, uint8_t reg,
			 uint8_t mask, uint8_t val);

#ifdef CONFIG_TPI1S1385_TRIGGER
int tpi1s1385_trigger_init(const struct device *dev);
int tpi1s1385_trigger_set(const struct device *dev,
			  const struct sensor_trigger *trig,
			  sensor_trigger_handler_t handler);
int tpi1s1385_attr_set(const struct device *dev,
		       enum sensor_channel chan,
		       enum sensor_attribute attr,
		       const struct sensor_value *val);
#endif /* CONFIG_TPI1S1385_TRIGGER */

#endif /* ZEPHYR_DRIVERS_SENSOR_EXCELITAS_TPI1S1385_TPI1S1385_H_ */
