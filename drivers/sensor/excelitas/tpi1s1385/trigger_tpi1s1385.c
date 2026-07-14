/*
 * Copyright (c) 2026 Semy BENADY
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Trigger support (GPIO interrupt) for the Excelitas TPiS 1S 1385.
 *
 * The device INT pin is open-drain, active low, and can be programmed to
 * assert on any combination of the following events:
 *   - Presence detected           (TPpresence)
 *   - Motion detected             (TPmotion)
 *   - Ambient temperature shock   (TPamb shock)
 *   - Over/under-temperature      (TPOT)
 *   - Internal timer
 *
 * The event selection is controlled through the INTERRUPT_MASK register
 * (0x19). Latched flags in INTERRUPT_STATUS (0x12) are cleared as soon as
 * that register is read.
 */

#include "tpi1s1385.h"
#include "tpi1s1385_priv.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(TPI1S1385, CONFIG_SENSOR_LOG_LEVEL);

static inline void setup_int(const struct device *dev, bool enable)
{
	const struct tpi1s1385_config *cfg = dev->config;
	int ret;

	ret = gpio_pin_interrupt_configure_dt(&cfg->int_gpio,
					      enable
					      ? GPIO_INT_EDGE_TO_ACTIVE
					      : GPIO_INT_DISABLE);
	if (ret < 0) {
		LOG_ERR("Failed to configure interrupt (ret %d)", ret);
	}
}

int tpi1s1385_attr_set(const struct device *dev,
		       enum sensor_channel chan,
		       enum sensor_attribute attr,
		       const struct sensor_value *val)
{
	const struct tpi1s1385_config *cfg = dev->config;
	int64_t value;
	uint8_t reg;

	if (!cfg->int_gpio.port) {
		return -ENOTSUP;
	}

	if (chan != SENSOR_CHAN_PROX) {
		return -ENOTSUP;
	}

	if (attr == SENSOR_ATTR_LOWER_THRESH) {
		reg = TPI1S1385_REG_PRESENCE_THRESHOLD;
	} else {
		return -ENOTSUP;
	}

	value = (int64_t)val->val1;

	if (tpi1s1385_reg_write(dev, reg, (uint8_t)value) < 0) {
		LOG_DBG("Failed to set attribute");
		return -EIO;
	}

	return 0;
}

static void tpi1s1385_gpio_callback(const struct device *dev,
				    struct gpio_callback *cb, uint32_t pins)
{
	struct tpi1s1385_data *drv_data =
		CONTAINER_OF(cb, struct tpi1s1385_data, gpio_cb);

	ARG_UNUSED(dev);
	ARG_UNUSED(pins);

	setup_int(drv_data->dev, false);

#if defined(CONFIG_TPI1S1385_TRIGGER_OWN_THREAD)
	k_sem_give(&drv_data->gpio_sem);
#elif defined(CONFIG_TPI1S1385_TRIGGER_GLOBAL_THREAD)
	k_work_submit(&drv_data->work);
#endif
}

static void tpi1s1385_thread_cb(const struct device *dev)
{
	struct tpi1s1385_data *drv_data = dev->data;
	uint8_t source;

	if (tpi1s1385_reg_read(dev, TPI1S1385_REG_INTERRUPT_STATUS,
			       &source) < 0) {
		return;
	}

	if ((source & TPI1S1385_INT_TP_MOTION) &&
	    drv_data->motion_handler != NULL) {
		drv_data->motion_handler(dev, drv_data->motion_trig);
	}

	if ((source & TPI1S1385_INT_TP_PRESENCE) &&
	    drv_data->presence_handler != NULL) {
		drv_data->presence_handler(dev, drv_data->presence_trig);
	}

	setup_int(dev, true);
}

#ifdef CONFIG_TPI1S1385_TRIGGER_OWN_THREAD
static void tpi1s1385_thread(void *p1, void *p2, void *p3)
{
	struct tpi1s1385_data *drv_data = p1;

	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	while (1) {
		k_sem_take(&drv_data->gpio_sem, K_FOREVER);
		tpi1s1385_thread_cb(drv_data->dev);
	}
}
#endif

#ifdef CONFIG_TPI1S1385_TRIGGER_GLOBAL_THREAD
static void tpi1s1385_work_cb(struct k_work *work)
{
	struct tpi1s1385_data *drv_data =
		CONTAINER_OF(work, struct tpi1s1385_data, work);

	tpi1s1385_thread_cb(drv_data->dev);
}
#endif

int tpi1s1385_trigger_set(const struct device *dev,
			  const struct sensor_trigger *trig,
			  sensor_trigger_handler_t handler)
{
	struct tpi1s1385_data *drv_data = dev->data;
	const struct tpi1s1385_config *cfg = dev->config;

	if (!cfg->int_gpio.port) {
		return -ENOTSUP;
	}

	setup_int(dev, false);

	if (trig->type == SENSOR_TRIG_MOTION) {
		drv_data->motion_handler = handler;
		drv_data->motion_trig = trig;
	} else if (trig->type == SENSOR_TRIG_NEAR_FAR) {
		drv_data->presence_handler = handler;
		drv_data->presence_trig = trig;
	}

	setup_int(dev, true);

	return 0;
}

int tpi1s1385_trigger_init(const struct device *dev)
{
	struct tpi1s1385_data *drv_data = dev->data;
	const struct tpi1s1385_config *cfg = dev->config;
	uint8_t mask = TPI1S1385_INT_TP_MOTION;

	/*
	 * Only enable the motion hardware interrupt by default. This avoids
	 * spurious wake-ups from the presence and timer sources; users that
	 * need them can enable additional bits at runtime by writing to
	 * INTERRUPT_MASK through tpi1s1385_reg_update().
	 */
	if (tpi1s1385_reg_update(dev, TPI1S1385_REG_INTERRUPT_MASK,
				 mask, mask) < 0) {
		LOG_DBG("Failed to enable interrupt source");
		return -EIO;
	}

	drv_data->dev = dev;

	if (!gpio_is_ready_dt(&cfg->int_gpio)) {
		LOG_ERR("INT GPIO device not ready");
		return -ENODEV;
	}

	gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);

	gpio_init_callback(&drv_data->gpio_cb,
			   tpi1s1385_gpio_callback,
			   BIT(cfg->int_gpio.pin));

	if (gpio_add_callback(cfg->int_gpio.port, &drv_data->gpio_cb) < 0) {
		LOG_DBG("Failed to set GPIO callback");
		return -EIO;
	}

#if defined(CONFIG_TPI1S1385_TRIGGER_OWN_THREAD)
	k_sem_init(&drv_data->gpio_sem, 0, K_SEM_MAX_LIMIT);

	k_thread_create(&drv_data->thread, drv_data->thread_stack,
			CONFIG_TPI1S1385_THREAD_STACK_SIZE,
			tpi1s1385_thread, drv_data, NULL, NULL,
			K_PRIO_COOP(CONFIG_TPI1S1385_THREAD_PRIORITY),
			0, K_NO_WAIT);
#elif defined(CONFIG_TPI1S1385_TRIGGER_GLOBAL_THREAD)
	k_work_init(&drv_data->work, tpi1s1385_work_cb);
#endif

	return 0;
}
