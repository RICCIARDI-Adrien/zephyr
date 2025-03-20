#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/printk.h>

#include "watchdog.h"

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
K_THREAD_DEFINE(thread_background, 256, thread_background_entry, NULL, NULL, NULL, -1, K_ESSENTIAL, 1);

int main(void)
{
	int states_count, ret, channel;
	const struct pm_state_info *states;
	unsigned int i = 0;
	uint32_t start_time = 0, current_time;

	ret = watchdog_init();
	if (ret < 0)
		return -1;

	printk("Hello world from %s\n", CONFIG_BOARD_TARGET);

	// Retrieving power states
	states_count = pm_state_cpu_get_all(0, &states);
	printk("Power states count : %d.\n", states_count);

	channel = task_wdt_add(2000, watchdog_callback, "main");
	if (channel < 0)
	{
		printk("Failed to initialize the watchdog for background thread; err=%d", -channel);
		return -1;
	}

	while (1)
	{
		printk("MAIN %u\n", i);
		i++;

		// Wait 1 second without sleeping, so we do not enter any power management state
		do
		{
			current_time = k_uptime_get_32();
		} while ((current_time - start_time) < 1000);
		start_time = current_time;

		task_wdt_feed(channel);
	}

	return 0;
}
