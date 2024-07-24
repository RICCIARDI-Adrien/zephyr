#include <zephyr/linker/sections.h>
#include <zephyr/devicetree.h>

#include <zephyr/linker/linker-defs.h>
#include <zephyr/linker/linker-tool.h>

MEMORY
{
	NVS_MEMORY (rw) : ORIGIN = 0x0803e800, LENGTH = 6144
}

/*SECTION_PROLOGUE(nvs_partition_0,,)
{
	*(".nvs_partition_0")
} GROUP_ROM_LINK_IN(NVS_MEMORY)*/

/*SECTIONS
{
	.nvs_partition_0 :
	{
		*(".nvs_partition_0")
	} > NVS_MEMORY
}*/

#include <zephyr/arch/arm/cortex_m/scripts/linker.ld>
