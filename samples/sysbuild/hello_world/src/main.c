/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/printk.h>

// Need to be included later because it depends on specific Zephyr headers (TODO fix)
#include <nvs_area.h>

#define NVS_PARTITION storage_partition

int main(void)
{
	int rc = 0, i;
	struct nvs_fs fs;
	struct flash_pages_info flash_info;
	uint8_t buf[32];
	uint16_t word;
	uint32_t double_word;

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
		printk("ID 12 (string equal to \"test string\") : \"%s\".\n", buf);
	}

	rc = nvs_read(&fs, 0, &word, sizeof(word));
	if (rc < 0)
		printk("Failed to read ID 0 (%s)\n", strerror(-rc));
	else
		printk("ID 0 (16-bit integer equal to 0x1234) : 0x%X.\n", word);

	rc = nvs_read(&fs, 3, &double_word, sizeof(double_word));
	if (rc < 0)
		printk("Failed to read ID 3 (%s)\n", strerror(-rc));
	else
		printk("ID 3 (32-bit integer equal to 0x5678) : 0x%X.\n", double_word);

	rc = nvs_read(&fs, 27349, buf, sizeof(buf));
	if (rc < 0)
		printk("Failed to read ID 27349 (%s)\n", strerror(-rc));
	else {
		printk("ID 27349 (6-byte array equal to : 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF) : ");
		for (i = 0; i < 6; i++)
			printk("0x%X, ", buf[i]);
		printk("\n");
	}

	return 0;
}
