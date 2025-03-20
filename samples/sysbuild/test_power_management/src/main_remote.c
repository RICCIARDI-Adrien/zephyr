#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/printk.h>

#include "watchdog.h"

static const struct gpio_dt_spec radio_led = GPIO_DT_SPEC_GET(DT_NODELABEL(radio_led), gpios);
static const struct gpio_dt_spec radio_button_suspend = GPIO_DT_SPEC_GET(DT_NODELABEL(radio_button_suspend), gpios);
static const struct gpio_dt_spec radio_button_resume = GPIO_DT_SPEC_GET(DT_NODELABEL(radio_button_resume), gpios);

static struct gpio_callback suspend_radio_button_callback_data;
static struct gpio_callback resume_radio_button_callback_data;

static void __attribute__((unused)) burn_cpu(void)
{
	volatile int a = 0, i;

	for (i = 0; i < 50000000; i++)
	{
		a += 1239;
		a *= -7;
		a /= 11;
	}
}

static void thread_background_entry(void *p1, void *p2, void *p3)
{
	unsigned int i = 0;
	int channel;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	channel = task_wdt_add(2000, watchdog_callback, "background");
	if (channel < 0)
	{
		printk("Failed to initialize the watchdog for background thread; err=%d", -channel);
		return;
	}

	printk("Background thread is ready.\n");

	while (1)
	{
		printk("BACKGROUND %u\n", i);
		i++;

		task_wdt_feed(channel);
		k_msleep(1000);
	}
}
// Higher priority thread to be able to interrupt the busy waits in the main thread
K_THREAD_DEFINE(thread_background, 512, thread_background_entry, NULL, NULL, NULL, -1, K_ESSENTIAL, 1);

static k_tid_t thread_id_main;

static void notifier_exit(enum pm_state state)
{
	printk("Resuming...\n");
}
static struct pm_notifier notifier = { .state_exit = notifier_exit };

static void gpio_callback(const struct device *port, struct gpio_callback *cb, gpio_port_pins_t pins)
{
	ARG_UNUSED(port);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	printk("\033[35mGPIO interrupt callback.\033[0m\n");

	if (pins & (1 << 2))
	{
		printk("\033[35mSUSPEND\033[0m\n");
		task_wdt_suspend();
		k_thread_suspend(thread_background);
		k_thread_suspend(thread_id_main);
	}
	else if (pins & (1 << 3))
	{
		printk("\033[35mRESUME\033[0m\n");
		task_wdt_resume();
		k_thread_resume(thread_id_main);
		k_thread_resume(thread_background);
	}
}

int main(void)
{
	int states_count, ret;
	const struct pm_state_info *states;
	unsigned int i = 0;

	ret = watchdog_init();
	if (ret < 0)
		return -1;

	thread_id_main = k_current_get();

	pm_notifier_register(&notifier);

	printk("Hello world from %s\n", CONFIG_BOARD_TARGET);

	// Retrieving power states
	states_count = pm_state_cpu_get_all(0, &states);
	printk("Power states count : %d.\n", states_count);

	if (!gpio_is_ready_dt(&radio_led))
	{
		printk("Radio LED GPIO is not ready.\n");
		return -1;
	}
	if (!gpio_is_ready_dt(&radio_button_resume))
	{
		printk("Suspend Radio button GPIO is not ready.\n");
		return -1;
	}
	if (!gpio_is_ready_dt(&radio_button_suspend))
	{
		printk("Resume Radio button GPIO is not ready.\n");
		return -1;
	}

	if (gpio_pin_configure_dt(&radio_led, GPIO_OUTPUT_ACTIVE) < 0)
	{
		printk("Failed to configure the Radio LED GPIO.\n");
		return -1;
	}
	ret = gpio_pin_configure_dt(&radio_button_resume, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0)
	{
		printk("Failed to configure the suspend Radio button GPIO (%d).\n", ret);
		return -1;
	}
	ret = gpio_pin_configure_dt(&radio_button_suspend, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0)
	{
		printk("Failed to configure the resume Radio button GPIO (%d).\n", ret);
		return -1;
	}

	ret = gpio_pin_interrupt_configure_dt(&radio_button_suspend, GPIO_INT_EDGE_FALLING);
	if (ret < 0)
	{
		printk("Failed to configure the suspend Radio button interrupt (%d).\n", ret);
		return -1;
	}
	ret = gpio_pin_interrupt_configure_dt(&radio_button_resume, GPIO_INT_EDGE_FALLING);
	if (ret < 0)
	{
		printk("Failed to configure the resume Radio button interrupt (%d).\n", ret);
		return -1;
	}

	gpio_init_callback(&suspend_radio_button_callback_data, gpio_callback, BIT(radio_button_suspend.pin));
	if (gpio_add_callback_dt(&radio_button_suspend, &suspend_radio_button_callback_data) < 0)
	{
		printk("Failed to add a callback to the suspend Radio button GPIO.\n");
		return -1;
	}
	gpio_init_callback(&resume_radio_button_callback_data, gpio_callback, BIT(radio_button_resume.pin));
	if (gpio_add_callback_dt(&radio_button_resume, &resume_radio_button_callback_data) < 0)
	{
		printk("Failed to add a callback to the resume Radio button GPIO.\n");
		return -1;
	}

	while (1)
	{
		printk("MAIN %u\n", i);
		i++;
		gpio_pin_toggle_dt(&radio_led);
		k_msleep(1000);
	}

	return 0;
}
