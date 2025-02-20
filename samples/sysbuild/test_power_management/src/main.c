/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <zephyr/sys/printk.h>

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

int main(void)
{
	int states_count;
	const struct pm_state_info *states;

	printk("Hello world from %s\n", CONFIG_BOARD_TARGET);

	// Retrieving power states
	states_count = pm_state_cpu_get_all(0, &states);
	printk("Power states count : %d.\n", states_count);


#ifdef NRF_RADIOCORE
	printk("Running for some seconds at 100%% CPU...\n");
	burn_cpu();

	printk("Forcing deep sleep state...\n");
	pm_state_force(0, &states[0]);
	k_msleep(1000);

	printk("Unexpected wake-up !\n");
	k_sleep(K_FOREVER);
#else
{
	volatile int a = 0;

	printk("Running forever at 100%% CPU...\n");

	while (1)
	{
		a += 1239;
		a *= -7;
		a /= 11;
	}
}
#endif

	return 0;
}
