#include <zephyr/linker/sections.h>
#include <zephyr/devicetree.h>

#include <zephyr/linker/linker-defs.h>
#include <zephyr/linker/linker-tool.h>

MEMORY
{
	NVS_MEMORY (rw) : ORIGIN = 0x0803e800, LENGTH = 6144
}

SECTIONS
{
	.nvs_partition :
	{
		KEEP(*(".nvs_partition"))
		KEEP(*(".nvs_partition.*"))
	} > NVS_MEMORY
}

#include <zephyr/arch/arm/cortex_m/scripts/linker.ld>
