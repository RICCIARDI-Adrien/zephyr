/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/can.h>

static const struct device *can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

int main(void)
{
	int ret, cnt = 1;
	struct can_frame frame;

	printf("Hello World CAN bordel! %s\n", CONFIG_BOARD_TARGET);

	// TEST
	k_sleep(K_FOREVER);

	ret = can_start(can_dev);
	if (ret != 0)
	{
		printf("err can_start() %d.\n", ret);
		return 0;
	}

	while (1)
	{
		printf("Envoi trame %d\n", cnt);
		cnt++;

		frame.id = 0x2CA;
		frame.dlc = 2;
		frame.flags = 0;
		frame.data[0] = 0xBA;
		frame.data[1] = 0xBE;
		ret = can_send(can_dev, &frame, K_FOREVER, NULL, NULL);
		if (ret != 0)
		{
			printf("err can_send() %d.\n", ret);
			return 0;
		}
		printf("\033[32mTrame envoyee\033[0m\n");

		k_msleep(2000);
	}

	return 0;
}
