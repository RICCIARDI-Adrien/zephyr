/*
 * Copyright (c) 2026 BayLibre <https://baylibre.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * GPIO based trigger support for the Excelitas TPiS 1S 1385. The INT pin
 * is open drain and active low. The device can raise interrupts on
 * presence, motion, ambient temperature shock, over temperature and its
 * internal timer. The set of sources that assert the INT pin is selected
 * by the INTERRUPT_MASK register. Reading INTERRUPT_STATUS clears the
 * latched flags.
 */

#include "tpi1s1385.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

LOG_MODULE_DECLARE(TPI1S1385, CONFIG_SENSOR_LOG_LEVEL);

/* Enables or disables the GPIO edge interrupt bound to the sensor INT pin. */
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
	struct tpi1s1385_data *drv_data = dev->data;
	const struct tpi1s1385_config *cfg = dev->config;
	int64_t value;
	uint8_t reg;
	int ret;

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

	k_mutex_lock(&drv_data->lock, K_FOREVER);
	ret = tpi1s1385_reg_write(dev, reg, (uint8_t)value);
	k_mutex_unlock(&drv_data->lock);

	if (ret < 0) {
		LOG_DBG("Failed to set attribute");
		return -EIO;
	}

	return 0;
}

/*
 * GPIO interrupt callback. Runs in interrupt context, masks the sensor
 * interrupt until the bottom half has processed it, then signals the
 * bottom half through either a semaphore or the system work queue.
 */
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

/*
 * Bottom half that runs in thread context. Reads INTERRUPT_STATUS to find
 * out which sources fired, takes a snapshot of the trigger handlers under
 * the spinlock so they cannot change while they are being invoked, calls
 * the matching handlers and finally re enables the GPIO interrupt.
 */
static void tpi1s1385_thread_cb(const struct device *dev)
{
	struct tpi1s1385_data *drv_data = dev->data;
	sensor_trigger_handler_t motion_handler, presence_handler;
	const struct sensor_trigger *motion_trig, *presence_trig;
	k_spinlock_key_t key;
	uint8_t source;

	k_mutex_lock(&drv_data->lock, K_FOREVER);
	if (tpi1s1385_reg_read(dev, TPI1S1385_REG_INTERRUPT_STATUS,
			       &source) < 0) {
		k_mutex_unlock(&drv_data->lock);
		return;
	}
	k_mutex_unlock(&drv_data->lock);

	key = k_spin_lock(&drv_data->trigger_lock);
	motion_handler = drv_data->motion_handler;
	motion_trig = drv_data->motion_trig;
	presence_handler = drv_data->presence_handler;
	presence_trig = drv_data->presence_trig;
	k_spin_unlock(&drv_data->trigger_lock, key);

	if ((source & TPI1S1385_INT_TP_MOTION) && motion_handler != NULL) {
		motion_handler(dev, motion_trig);
	}

	if ((source & TPI1S1385_INT_TP_PRESENCE) && presence_handler != NULL) {
		presence_handler(dev, presence_trig);
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
	k_spinlock_key_t key;

	if (!cfg->int_gpio.port) {
		return -ENOTSUP;
	}

	k_mutex_lock(&drv_data->lock, K_FOREVER);
	setup_int(dev, false);

	key = k_spin_lock(&drv_data->trigger_lock);
	if (trig->type == SENSOR_TRIG_MOTION) {
		drv_data->motion_handler = handler;
		drv_data->motion_trig = trig;
	} else if (trig->type == SENSOR_TRIG_NEAR_FAR) {
		drv_data->presence_handler = handler;
		drv_data->presence_trig = trig;
	}
	k_spin_unlock(&drv_data->trigger_lock, key);

	setup_int(dev, true);
	k_mutex_unlock(&drv_data->lock);

	return 0;
}

int tpi1s1385_trigger_init(const struct device *dev)
{
	struct tpi1s1385_data *drv_data = dev->data;
	const struct tpi1s1385_config *cfg = dev->config;
	uint8_t mask = TPI1S1385_INT_TP_MOTION;

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

	/*
	 * Only the motion source is enabled by default. This keeps the INT
	 * pin quiet for the presence and timer sources, which the user can
	 * turn on at runtime by writing to INTERRUPT_MASK.
	 */
	if (tpi1s1385_reg_update(dev, TPI1S1385_REG_INTERRUPT_MASK,
				 mask, mask) < 0) {
		LOG_DBG("Failed to enable interrupt source");
		return -EIO;
	}

	/*
	 * The GPIO interrupt is enabled last, once the bottom half is ready
	 * to service it.
	 */
	setup_int(dev, true);

	return 0;
}
