/*
 * Copyright (c) 2026 Semy BENADY
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_SENSOR_EXCELITAS_TPI1S1385_TPI1S1385_PRIV_H_
#define ZEPHYR_DRIVERS_SENSOR_EXCELITAS_TPI1S1385_TPI1S1385_PRIV_H_

#include "tpi1s1385.h"
#include <zephyr/kernel.h>

#ifdef CONFIG_TPI1S1385_TRIGGER
#include <zephyr/drivers/gpio.h>
#endif

/* Calibration constants (datasheet sections 8.1 and 8.2). */
struct tpi1s1385_eeprom {
	uint16_t ptat25;   /* Reg 42-43: TPamb output in digits at 25 degC (15 bits). */
	uint16_t m;        /* Reg 44-45: PTAT slope in digits/K scaled by 100. */
	uint16_t u0;       /* Reg 46-47: TPobject offset. */
	uint16_t uout1;    /* Reg 48-49: TPobject output for the TOBJ1 reference. */
	uint8_t  tobj1;    /* Reg 50: black-body reference temperature in degC. */
	uint8_t  lookup;   /* Reg 41: lookup table identifier. */
};

struct tpi1s1385_config {
	struct i2c_dt_spec i2c;
#ifdef CONFIG_TPI1S1385_TRIGGER
	struct gpio_dt_spec int_gpio;
#endif
	/* Device Tree configuration parameters (datasheet sections 6.2, 7.2, 7.3). */
	uint8_t slp1;               /* LP1 time constant (4 bits, reg 0x14[3:0]). */
	uint8_t slp2;               /* LP2 time constant (4 bits, reg 0x14[7:4]). */
	uint8_t slp3;               /* LP3 time constant (4 bits, reg 0x15[3:0]). */
	uint8_t src_select;         /* Presence source select (2 bits, reg 0x1A[3:2]). */
	uint8_t cycle_time;         /* Motion cycle time (2 bits, reg 0x1A[1:0]). */
	uint8_t tpot_direction;     /* TPOT direction (1 bit, reg 0x1A[4]). */
	uint8_t presence_threshold; /* Presence threshold (8 bits, reg 0x16). */
	uint8_t motion_threshold;   /* Motion threshold (8 bits, reg 0x17). */
	uint8_t amb_shock_threshold;/* Ambient shock threshold (8 bits, reg 0x18). */
};

struct tpi1s1385_data {
	int32_t tp_object;
	int16_t tp_ambient;
	uint8_t tp_presence;
	uint8_t tp_motion;
	uint8_t interrupt_status;

	struct k_mutex lock;

	struct tpi1s1385_eeprom eeprom;

#ifdef CONFIG_TPI1S1385_TRIGGER
	const struct device *dev;
	struct gpio_callback gpio_cb;
	sensor_trigger_handler_t presence_handler;
	const struct sensor_trigger *presence_trig;
	sensor_trigger_handler_t motion_handler;
	const struct sensor_trigger *motion_trig;

#ifdef CONFIG_TPI1S1385_TRIGGER_OWN_THREAD
	K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_TPI1S1385_THREAD_STACK_SIZE);
	struct k_thread thread;
	struct k_sem gpio_sem;
#elif defined(CONFIG_TPI1S1385_TRIGGER_GLOBAL_THREAD)
	struct k_work work;
#endif
#endif /* CONFIG_TPI1S1385_TRIGGER */
};

#endif /* ZEPHYR_DRIVERS_SENSOR_EXCELITAS_TPI1S1385_TPI1S1385_PRIV_H_ */
