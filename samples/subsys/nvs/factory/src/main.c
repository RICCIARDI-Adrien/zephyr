/*
 * Copyright (C) 2024 BayLibre.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

static int mount_nvs(struct nvs_fs *fs)
{
	struct flash_pages_info flash_info;
	size_t partition_size;
	off_t offset;
	int i, ret;

	fs->flash_device = FIXED_PARTITION_DEVICE(FACTORY_NVS_PARTITION_DT_LABEL);
	if (!device_is_ready(fs->flash_device)) {
		printk("Flash device %s is not ready.\n", fs->flash_device->name);
		return -1;
	}
	fs->offset = FIXED_PARTITION_OFFSET(FACTORY_NVS_PARTITION_DT_LABEL);

	ret = flash_get_page_info_by_offs(fs->flash_device, fs->offset, &flash_info);
	if (ret) {
		printk("Unable to get flash page info.\n");
		return ret;
	}
	partition_size = FIXED_PARTITION_SIZE(FACTORY_NVS_PARTITION_DT_LABEL);
	fs->sector_size = flash_info.size;
	fs->sector_count = partition_size / fs->sector_size;

	// Make sure the flash partition is cleared before starting
	offset = fs->offset;
	for (i = 0; i < fs->sector_count; i++) {
		ret = flash_erase(fs->flash_device, offset, fs->sector_size);
		if (ret) {
			printk("flash_erase() failed for sector %d (%s).\n", i, strerror(-ret));
			return ret;
		}
		offset += fs->sector_size;
	}

	ret = nvs_mount(fs);
	if (ret) {
		printk("mvs_mount() failed (%s).\n", strerror(-ret));
		return ret;
	}

	return 0;
}

int main(void)
{
	struct nvs_fs fs;
	int ret;

	printk("Creating an empty NVS...\n");
	ret = mount_nvs(&fs);

	return 0;
}
