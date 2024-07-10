#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>

#include "nsi_cmdline.h"

// TEST
#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

int main(void)
{
	struct nvs_fs fs;
	struct flash_pages_info flash_info;
	int ret;
	int argc; char **argv;

	nsi_get_test_cmd_line_args(&argc, &argv);
	printk("argc=%d\n", argc);
	for (ret = 0; ret < argc; ret++) {
		printk("arg %d = %s\n", ret, argv[ret]);
	}

	fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs.flash_device)) {
		printk("Flash device %s is not ready\n", fs.flash_device->name);
		return 0;
	}
	fs.offset = NVS_PARTITION_OFFSET;
	ret = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &flash_info);
	if (ret) {
		printk("Unable to get page info\n");
		return 0;
	}
	fs.sector_size = flash_info.size;
	fs.sector_count = 3U;

	ret = nvs_mount(&fs);
	if (ret) {
		printk("Flash Init failed\n");
		return 0;
	}

	return 0;
}
