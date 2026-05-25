/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/drivers/can.h>

#define ENABLE_CAN_1 1

//static const struct device *can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

static const struct device *can_dev = DEVICE_DT_GET(DT_NODELABEL(can0));
#if ENABLE_CAN_1
static const struct device *can_dev_1 = DEVICE_DT_GET(DT_NODELABEL(can1));
#endif

static void can_rx_callback(const struct device *dev, struct can_frame *frame, void *user_data)
{
	int i, bytes;
	uint8_t *ptr_data;

	bytes = can_dlc_to_bytes(frame->dlc);
	ptr_data = frame->data;

	printk("Trame reçue %s ! ID=%X, DLC=%u, %s%s%sbytes=%d.\n", dev->name, frame->id, frame->dlc,
		frame->flags & CAN_FRAME_IDE ? "IDE, " : "",
		frame->flags & CAN_FRAME_FDF ? "FDF, " : "",
		frame->flags & CAN_FRAME_BRS ? "BRS, " : "",
		bytes);
	for (i = 0; i < bytes; i++)
	{
		printk("Data %i : %02X\n", i, *ptr_data);
		ptr_data++;
	}
}

int main(void)
{
	int ret, cnt = 1;
	struct can_frame frame;
	struct can_filter filter;
	struct can_timing timing;

	printk("TEST CAN sur %s\n", CONFIG_BOARD_TARGET);

	ret = can_set_mode(can_dev, CAN_MODE_LOOPBACK);
	if (ret != 0)
	{
		printk("err can_set_mode() %d.\n", ret);
		return 0;
	}

	ret = can_start(can_dev);
	if (ret != 0)
	{
		printk("err can_start() %d.\n", ret);
		return 0;
	}

	ret = can_stop(can_dev);
	if (ret != 0)
	{
		printk("err can_stop() %d.\n", ret);
		return 0;
	}

	ret = can_set_mode(can_dev, CAN_MODE_NORMAL);
	if (ret != 0)
	{
		printk("err can_set_mode() %d.\n", ret);
		return 0;
	}

	ret = can_calc_timing(can_dev, &timing, 1000000, 0);
	if (ret < 0)
	{
		printk("err can_calc_timing() %d.\n", ret);
		return 0;
	}

	ret = can_set_timing(can_dev, &timing);
	if (ret != 0)
	{
		printk("err can_set_timing() %d.\n", ret);
		return 0;
	}

#ifdef CONFIG_CAN_FD_MODE
	ret = can_calc_timing_data(can_dev, &timing, 8000000, 0);
	if (ret < 0)
	{
		printk("err can_calc_timing_data() %d.\n", ret);
		return 0;
	}

	ret = can_set_timing_data(can_dev, &timing);
	if (ret != 0)
	{
		printk("err can_set_timing() %d.\n", ret);
		return 0;
	}

	ret = can_set_mode(can_dev, CAN_MODE_FD);
	if (ret != 0)
	{
		printk("err can_set_mode FD () %d.\n", ret);
		return 0;
	}
#endif

	ret = can_start(can_dev);
	if (ret != 0)
	{
		printk("err can_start() %d.\n", ret);
		return 0;
	}

	// Receive all normal ID frames
	filter.id = 0;
	filter.mask = 0;
	filter.flags = 0;
	ret = can_add_rx_filter(can_dev, can_rx_callback, NULL, &filter);
	if (ret < 0)
	{
		printk("err can_add_rx_filter() %d.\n", ret);
		return 0;
	}
	printk("CAN 0 RX filter ID : %d.\n", ret);

	// Receive all IDE frames
	filter.id = 0;
	filter.mask = 0;
	filter.flags = CAN_FRAME_IDE;
	ret = can_add_rx_filter(can_dev, can_rx_callback, NULL, &filter);
	if (ret < 0)
	{
		printk("err can_add_rx_filter() %d.\n", ret);
		return 0;
	}
	printk("CAN 0 RX filter ID : %d.\n", ret);

#if ENABLE_CAN_1
	ret = can_start(can_dev_1);
	if (ret != 0)
	{
		printk("err can_start() dev 1 %d.\n", ret);
		return 0;
	}

	// Receive all frames
	filter.id = 0;
	filter.mask = 0;
	filter.flags = 0;
	ret = can_add_rx_filter(can_dev_1, can_rx_callback, NULL, &filter);
	if (ret < 0)
	{
		printk("err can_add_rx_filter() dev 1 %d.\n", ret);
		return 0;
	}
	printk("CAN 1 RX filter ID : %d.\n", ret);
#endif

	while (1)
	{
		printk("\033[32mEnvoi trame %d\033[0m\n", cnt);
		cnt++;

		frame.id = 0x3F0;
		frame.dlc = can_bytes_to_dlc(4);
		frame.flags = 0;
#if CONFIG_CAN_FD_MODE
		frame.flags |= CAN_FRAME_FDF | CAN_FRAME_BRS;
#endif
		frame.data[0] = cnt >> 24;
		frame.data[1] = cnt >> 16;
		frame.data[2] = cnt >> 8;
		frame.data[3] = (unsigned char) cnt;
		ret = can_send(can_dev, &frame, K_FOREVER, NULL, NULL);
		if (ret != 0)
		{
			printk("err can_send() %d.\n", ret);
			//return 0;
		}

#if ENABLE_CAN_1
		frame.id = 0x3F1;
		frame.dlc = can_bytes_to_dlc(4);
		frame.flags = 0;
		frame.data[0] = cnt >> 24;
		frame.data[1] = cnt >> 16;
		frame.data[2] = cnt >> 8;
		frame.data[3] = (unsigned char) cnt;
		ret = can_send(can_dev_1, &frame, K_FOREVER, NULL, NULL);
		if (ret != 0)
		{
			printk("err can_send() dev 1 %d.\n", ret);
			//return 0;
		}
#endif

		k_msleep(3000);
	}

	return 0;
}
