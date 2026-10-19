/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
	printf("k_cycle_get_32()=%u\n", k_cycle_get_32());

	printf("Z_SPIN_DELAY(10)\n");
	Z_SPIN_DELAY(10);

	uint32_t _wf_cycle_count = k_us_to_cyc_ceil32(10);
	printf("k_us_to_cyc_ceil32(10)=%u\n", _wf_cycle_count);

	printf("k_sleep_ticks\n");
	k_ticks_t ticks = k_sleep_ticks(K_USEC(10));

	//printk("msleep\n");
	//k_msleep(3);

	printf("FIN OK\n");

	return 0;
}
