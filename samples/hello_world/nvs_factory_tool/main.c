#include <stdio.h>

#include <zephyr/autoconf.h>
#include <zephyr/devicetree.h>

int main(int argc, char *argv[])
{
	printf("DT value = 0x%x.\n", DT_PROP(DT_NODELABEL(can0), device_id));
	printf("Kconfig value = %s.\n", CONFIG_KERNEL_BIN_NAME);

	return 0;
}
