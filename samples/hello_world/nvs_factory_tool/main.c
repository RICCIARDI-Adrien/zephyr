#include <stdio.h>

#include <nvs_factory_tool.h>
#include <zephyr/autoconf.h>
#include <zephyr/devicetree.h>

int main(int argc, char *argv[])
{
	printf("DT value = 0x%x.\n", DT_PROP(DT_NODELABEL(can0), device_id));
	printf("Kconfig value = %s.\n", CONFIG_KERNEL_BIN_NAME);

	printf("Layer 0 info : DT label=%s, items count=%zu.\n",
	       layers[0].storage_partition_device_tree_label,
	       layers[0].items_count);

	return 0;
}
