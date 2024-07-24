/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/printk.h>

__in_section(nvs_partition, static, var) unsigned int test[100] = {
	0x1234, 0x5678, 0x1000
};

#define NVS_PARTITION storage_partition

int main(void)
{
	int rc = 0;
	struct nvs_fs fs;
	struct flash_pages_info flash_info;
	uint8_t buf[32];

	printk("Hello world from %s\n", CONFIG_BOARD_TARGET);

	fs.flash_device = FIXED_PARTITION_DEVICE(NVS_PARTITION);
	fs.offset = FIXED_PARTITION_OFFSET(NVS_PARTITION);
	rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &flash_info);
	if (rc) {
		printk("Unable to get page info\n");
		return 0;
	}
	fs.sector_size = flash_info.size;
	fs.sector_count = FIXED_PARTITION_SIZE(NVS_PARTITION) / fs.sector_size;

	rc = nvs_mount(&fs);
	if (rc) {
		printk("Flash init failed\n");
		return 0;
	}

	rc = nvs_read(&fs, 12, buf, sizeof(buf));
	if (rc < 0)
		printk("Failed to read ID 12 (%s)\n", strerror(-rc));
	else {
		buf[sizeof(buf) - 1] = 0;
		printf("ID 12 : \"%s\".\n", buf);
	}

	return 0;
}
