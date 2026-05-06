/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/can.h>

//static const struct device *can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static const struct device *can_dev = DEVICE_DT_GET(DT_NODELABEL(can0)); //DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));
static const struct device *can_dev_1 = DEVICE_DT_GET(DT_NODELABEL(can1));

int main(void)
{
	int ret, cnt = 1;
	struct can_frame frame;

	printk("Hello World CAN bordel! %s\n", CONFIG_BOARD_TARGET);

	/*ret = can_set_mode(can_dev, CAN_MODE_NORMAL);
	if (ret != 0)
	{
		printk("err can_set_mode() %d.\n", ret);
		return 0;
	}*/

	ret = can_start(can_dev);
	if (ret != 0)
	{
		printk("err can_start() %d.\n", ret);
		return 0;
	}

	ret = can_start(can_dev_1);
	if (ret != 0)
	{
		printk("err can_start dev 1() %d.\n", ret);
		return 0;
	}

	while (1)
	{
		printk("\033Envoi trame %d\033[0m\n", cnt);
		cnt++;

		frame.id = 0x3F0;
		frame.dlc = can_bytes_to_dlc(4);
		frame.flags = 0;
		frame.data[0] = cnt >> 24;
		frame.data[1] = cnt >> 16;
		frame.data[2] = cnt >> 8;
		frame.data[3] = (unsigned char) cnt;
		ret = can_send(can_dev, &frame, K_FOREVER, NULL, NULL);
		if (ret != 0)
		{
			printk("err can_send() %d.\n", ret);
			return 0;
		}

		/*frame.id = 0x3F1;
		frame.dlc = can_bytes_to_dlc(2);
		frame.flags = 0;
		frame.data[0] = 0xBA;
		frame.data[1] = 0xBE;
		ret = can_send(can_dev_1, &frame, K_FOREVER, NULL, NULL);
		if (ret != 0)
		{
			printk("err can_send() %d.\n", ret);
			return 0;
		}
		printk("\033[32mTrame envoyee\033[0m\n");*/

		k_msleep(100);
	}

	return 0;
}
