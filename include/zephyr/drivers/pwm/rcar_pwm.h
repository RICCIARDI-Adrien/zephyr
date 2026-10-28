/*
 * Copyright (c) 2026 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Header file for the R-Car PWM driver
 * @ingroup pwm_rcar_interface
 */

#ifndef ZEPHYR_INCLUDE_DRIVERS_PWM_RCAR_PWM_H_
#define ZEPHYR_INCLUDE_DRIVERS_PWM_RCAR_PWM_H_

/**
 * @defgroup pwm_rcar_interface R-Car
 * @ingroup pwm_interface_ext
 * @brief Renesas R-Car PWM controller
 * @{
 */

/** @cond INTERNAL_HIDDEN */
#define RCAR_PWM_CYCLES_SHIFT_POS  8
#define RCAR_PWM_CYCLES_SHIFT_MASK (0x1f << RCAR_PWM_CYCLES_SHIFT_POS)
/** @endcond */

/**
 * @brief Count the period and pulse in units of 2^n cycles, on Gen5
 *
 * For pwm_set_cycles() only, in the upper 8 bits of pwm_flags_t. A 32 bit
 * period then goes up to 2^(32 + n) counter cycles, and n = 16 reaches the 2^24
 * divider of the hardware. pwm_set() and pwm_set_dt() convert nanoseconds with
 * the undivided rate, so with this flag their period would be 2^n times longer.
 */
#define RCAR_PWM_CYCLES_SHIFT(n) ((n) << RCAR_PWM_CYCLES_SHIFT_POS)

/** @} */

#endif /* ZEPHYR_INCLUDE_DRIVERS_PWM_RCAR_PWM_H_ */
