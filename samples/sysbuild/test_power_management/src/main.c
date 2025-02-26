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

static void watchdog_callback(int channel_id, void *user_data)
{
	char *cpu_name;
#ifdef NRF_RADIOCORE
	cpu_name = "Radio";
#else
	cpu_name = "App";
#endif

	printk("Task watchdog fired for channel %d on %s core !\n", channel_id, cpu_name);

	// Wait for the hardware watchdog to reset the SoC
	while (1);
}

#ifdef NRF_RADIOCORE
static void thread_background_entry(void *p1, void *p2, void *p3)
{
	unsigned int i = 0;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	printk("Background thread is ready.\n");

	while (1)
	{
		printk("BACKGROUND %u\n", i);
		i++;
		k_msleep(10000);
	}
}
// Preemptive thread
K_THREAD_DEFINE(thread_background, 256, thread_background_entry, NULL, NULL, NULL, 2, K_ESSENTIAL, 1);
#endif

static k_tid_t thread_id_main;

#ifdef NRF_RADIOCORE
static void notifier_exit(enum pm_state state)
{
	printk("Resuming...\n");
	k_thread_resume(thread_id_main);
	k_thread_resume(thread_background);
}

static struct pm_notifier notifier = { .state_exit = notifier_exit };
#endif

int main(void)
{
	int states_count, ret;
	const struct pm_state_info *states;

#ifndef NRF_RADIOCORE
	ret = watchdog_init();
	if (ret < 0)
		return -1;
#endif

	thread_id_main = k_current_get();
#ifdef NRF_RADIOCORE
	pm_notifier_register(&notifier);
#endif

	printk("Hello world from %s\n", CONFIG_BOARD_TARGET);

	// Retrieving power states
	states_count = pm_state_cpu_get_all(0, &states);
	printk("Power states count : %d.\n", states_count);


#ifdef NRF_RADIOCORE
{
	unsigned int i = 0;

	printk("Running for some seconds at 100%% CPU...\n");
	//burn_cpu();

	printk("Forcing deep sleep state...\n");
	pm_state_force(0, &states[0]);
	k_thread_suspend(thread_background);
	k_thread_suspend(thread_id_main);

	printk("Wake-up !\n");
	while (1)
	{
		printk("MAIN %u\n", i);
		i++;
		k_msleep(8000);
	}
}
#else
{
	volatile int a = 0;
	int channel;
	uint32_t start_time = 0, current_time;

	printk("Running forever at 100%% CPU...\n");
	channel = task_wdt_add(2000, watchdog_callback, "main");
	if (channel < 0)
	{
		printk("Failed to initialize the watchdog for background thread; err=%d", -channel);
		return -1;
	}

	while (1)
	{
		a += 1239;
		a *= -7;
		a /= 11;

		current_time = k_uptime_get_32();
		if ((current_time - start_time) >= 1000)
		{
			start_time = current_time;
			task_wdt_feed(channel);
			printk("Watchdog feed.\n");
		}
	}
}
#endif

	return 0;
}
