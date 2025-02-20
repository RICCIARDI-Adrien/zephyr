/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/toolchain.h>
#include <zephyr/pm/policy.h>
#include <zephyr/arch/common/pm_s2ram.h>
#include <hal/nrf_resetinfo.h>
#include <hal/nrf_memconf.h>
#include <zephyr/cache.h>
#include <power.h>
#include <soc_lrcconf.h>
#include "soc.h"
#include "pm_s2ram.h"

extern sys_snode_t soc_node;

static void common_suspend(void)
{
	if (IS_ENABLED(CONFIG_DCACHE)) {
		/* Flush, disable and power down DCACHE */
		sys_cache_data_flush_all();
		sys_cache_data_disable();
		nrf_memconf_ramblock_control_enable_set(NRF_MEMCONF, RAMBLOCK_POWER_ID,
							RAMBLOCK_CONTROL_BIT_DCACHE, false);
	}

	if (IS_ENABLED(CONFIG_ICACHE)) {
		/* Disable and power down ICACHE */
		sys_cache_instr_disable();
		nrf_memconf_ramblock_control_enable_set(NRF_MEMCONF, RAMBLOCK_POWER_ID,
							RAMBLOCK_CONTROL_BIT_ICACHE, false);
	}

	soc_lrcconf_poweron_release(&soc_node, NRF_LRCCONF_POWER_DOMAIN_0);
}

static void common_resume(void)
{
	if (IS_ENABLED(CONFIG_ICACHE)) {
		/* Power up and re-enable ICACHE */
		nrf_memconf_ramblock_control_enable_set(NRF_MEMCONF, RAMBLOCK_POWER_ID,
							RAMBLOCK_CONTROL_BIT_ICACHE, true);
		sys_cache_instr_enable();
	}

	if (IS_ENABLED(CONFIG_DCACHE)) {
		/* Power up and re-enable DCACHE */
		nrf_memconf_ramblock_control_enable_set(NRF_MEMCONF, RAMBLOCK_POWER_ID,
							RAMBLOCK_CONTROL_BIT_DCACHE, true);
		sys_cache_data_enable();
	}

	soc_lrcconf_poweron_request(&soc_node, NRF_LRCCONF_POWER_DOMAIN_0);
}

void nrf_poweroff(void)
{
	nrf_resetinfo_resetreas_local_set(NRF_RESETINFO, 0);
	nrf_resetinfo_restore_valid_set(NRF_RESETINFO, false);

#if !defined(CONFIG_SOC_NRF54H20_CPURAD)
	/* Disable retention */
	nrf_lrcconf_retain_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_MAIN, false);
	nrf_lrcconf_retain_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_DOMAIN_0, false);
#endif
	common_suspend();

	nrf_lrcconf_task_trigger(NRF_LRCCONF010, NRF_LRCCONF_TASK_SYSTEMOFFREADY);

	__set_BASEPRI(0);
	__ISB();
	__DSB();
	__WFI();

	CODE_UNREACHABLE;
}

#if defined(NRF_RADIOCORE)
uint32_t pendings[93];
#else
uint32_t pendings[79];
#endif

static void s2idle_enter(uint8_t substate_id)
{
	printk("s2idle_enter substate_id=%u\n", substate_id);

	switch (substate_id) {
	case 0:
		/* Substate for idle with cache powered on - not implemented yet. */
		break;
	case 1: /* Substate for idle with cache retained - not implemented yet. */
		break;
	case 2: /* Substate for idle with cache disabled. */
#if !defined(CONFIG_SOC_NRF54H20_CPURAD)
		soc_lrcconf_poweron_request(&soc_node, NRF_LRCCONF_POWER_MAIN);
#endif
		common_suspend();
		printk("s2idle_enter fin code enter suspend\n");
		break;
	default: /* Unknown substate. */
		return;
	}

#if defined(NRF_RADIOCORE)
	pendings[0] = NVIC_GetPendingIRQ(SPU000_IRQn);
	pendings[1] = NVIC_GetPendingIRQ(MPC_IRQn);
	pendings[2] = NVIC_GetPendingIRQ(CPUC_IRQn);
	pendings[3] = NVIC_GetPendingIRQ(MVDMA_IRQn);
	pendings[4] = NVIC_GetPendingIRQ(SPU010_IRQn);
	pendings[5] = NVIC_GetPendingIRQ(WDT010_IRQn);
	pendings[6] = NVIC_GetPendingIRQ(WDT011_IRQn);
	pendings[7] = NVIC_GetPendingIRQ(SPU020_IRQn);
	pendings[8] = NVIC_GetPendingIRQ(EGU020_IRQn);
	pendings[9] = NVIC_GetPendingIRQ(GPIOTE_0_IRQn);
	pendings[10] = NVIC_GetPendingIRQ(TIMER020_IRQn);
	pendings[11] = NVIC_GetPendingIRQ(TIMER021_IRQn);
	pendings[12] = NVIC_GetPendingIRQ(TIMER022_IRQn);
	pendings[13] = NVIC_GetPendingIRQ(RTC_IRQn);
	pendings[14] = NVIC_GetPendingIRQ(RADIO_0_IRQn);
	pendings[15] = NVIC_GetPendingIRQ(RADIO_1_IRQn);
	pendings[16] = NVIC_GetPendingIRQ(SPU030_IRQn);
	pendings[17] = NVIC_GetPendingIRQ(AAR030_CCM030_IRQn);
	pendings[18] = NVIC_GetPendingIRQ(ECB030_IRQn);
	pendings[19] = NVIC_GetPendingIRQ(AAR031_CCM031_IRQn);
	pendings[20] = NVIC_GetPendingIRQ(ECB031_IRQn);
	pendings[21] = NVIC_GetPendingIRQ(IPCT_0_IRQn);
	pendings[22] = NVIC_GetPendingIRQ(IPCT_1_IRQn);
	pendings[23] = NVIC_GetPendingIRQ(CTI_0_IRQn);
	pendings[24] = NVIC_GetPendingIRQ(CTI_1_IRQn);
	pendings[25] = NVIC_GetPendingIRQ(SWI0_IRQn);
	pendings[26] = NVIC_GetPendingIRQ(SWI1_IRQn);
	pendings[27] = NVIC_GetPendingIRQ(SWI2_IRQn);
	pendings[28] = NVIC_GetPendingIRQ(SWI3_IRQn);
	pendings[29] = NVIC_GetPendingIRQ(SWI4_IRQn);
	pendings[30] = NVIC_GetPendingIRQ(SWI5_IRQn);
	pendings[31] = NVIC_GetPendingIRQ(SWI6_IRQn);
	pendings[32] = NVIC_GetPendingIRQ(SWI7_IRQn);
	pendings[33] = NVIC_GetPendingIRQ(BELLBOARD_0_IRQn);
	pendings[34] = NVIC_GetPendingIRQ(BELLBOARD_1_IRQn);
	pendings[35] = NVIC_GetPendingIRQ(BELLBOARD_2_IRQn);
	pendings[36] = NVIC_GetPendingIRQ(BELLBOARD_3_IRQn);
	pendings[37] = NVIC_GetPendingIRQ(GPIOTE130_0_IRQn);
	pendings[38] = NVIC_GetPendingIRQ(GPIOTE130_1_IRQn);
	pendings[39] = NVIC_GetPendingIRQ(GRTC_0_IRQn);
	pendings[40] = NVIC_GetPendingIRQ(GRTC_1_IRQn);
	pendings[41] = NVIC_GetPendingIRQ(GRTC_2_IRQn);
	pendings[42] = NVIC_GetPendingIRQ(TBM_IRQn);
	pendings[43] = NVIC_GetPendingIRQ(USBHS_IRQn);
	pendings[44] = NVIC_GetPendingIRQ(EXMIF_IRQn);
	pendings[45] = NVIC_GetPendingIRQ(IPCT120_0_IRQn);
	pendings[46] = NVIC_GetPendingIRQ(I3C120_IRQn);
	pendings[47] = NVIC_GetPendingIRQ(VPR121_IRQn);
	pendings[48] = NVIC_GetPendingIRQ(CAN120_IRQn);
	pendings[49] = NVIC_GetPendingIRQ(MVDMA120_IRQn);
	pendings[50] = NVIC_GetPendingIRQ(I3C121_IRQn);
	pendings[51] = NVIC_GetPendingIRQ(TIMER120_IRQn);
	pendings[52] = NVIC_GetPendingIRQ(TIMER121_IRQn);
	pendings[53] = NVIC_GetPendingIRQ(PWM120_IRQn);
	pendings[54] = NVIC_GetPendingIRQ(SPIS120_IRQn);
	pendings[55] = NVIC_GetPendingIRQ(SPIM120_UARTE120_IRQn);
	pendings[56] = NVIC_GetPendingIRQ(SPIM121_IRQn);
	pendings[57] = NVIC_GetPendingIRQ(VPR130_IRQn);
	pendings[58] = NVIC_GetPendingIRQ(IPCT130_0_IRQn);
	pendings[59] = NVIC_GetPendingIRQ(RTC130_IRQn);
	pendings[60] = NVIC_GetPendingIRQ(RTC131_IRQn);
	pendings[61] = NVIC_GetPendingIRQ(WDT131_IRQn);
	pendings[62] = NVIC_GetPendingIRQ(WDT132_IRQn);
	pendings[63] = NVIC_GetPendingIRQ(EGU130_IRQn);
	pendings[64] = NVIC_GetPendingIRQ(SAADC_IRQn);
	pendings[65] = NVIC_GetPendingIRQ(COMP_LPCOMP_IRQn);
	pendings[66] = NVIC_GetPendingIRQ(TEMP_IRQn);
	pendings[67] = NVIC_GetPendingIRQ(NFCT_IRQn);
	pendings[68] = NVIC_GetPendingIRQ(TDM130_IRQn);
	pendings[69] = NVIC_GetPendingIRQ(PDM_IRQn);
	pendings[70] = NVIC_GetPendingIRQ(QDEC130_IRQn);
	pendings[71] = NVIC_GetPendingIRQ(QDEC131_IRQn);
	pendings[72] = NVIC_GetPendingIRQ(TDM131_IRQn);
	pendings[73] = NVIC_GetPendingIRQ(TIMER130_IRQn);
	pendings[74] = NVIC_GetPendingIRQ(TIMER131_IRQn);
	pendings[75] = NVIC_GetPendingIRQ(PWM130_IRQn);
	pendings[76] = NVIC_GetPendingIRQ(SERIAL0_IRQn);
	pendings[77] = NVIC_GetPendingIRQ(SERIAL1_IRQn);
	pendings[78] = NVIC_GetPendingIRQ(TIMER132_IRQn);
	pendings[79] = NVIC_GetPendingIRQ(TIMER133_IRQn);
	pendings[80] = NVIC_GetPendingIRQ(PWM131_IRQn);
	pendings[81] = NVIC_GetPendingIRQ(SERIAL2_IRQn);
	pendings[82] = NVIC_GetPendingIRQ(SERIAL3_IRQn);
	pendings[83] = NVIC_GetPendingIRQ(TIMER134_IRQn);
	pendings[84] = NVIC_GetPendingIRQ(TIMER135_IRQn);
	pendings[85] = NVIC_GetPendingIRQ(PWM132_IRQn);
	pendings[86] = NVIC_GetPendingIRQ(SERIAL4_IRQn);
	pendings[87] = NVIC_GetPendingIRQ(SERIAL5_IRQn);
	pendings[88] = NVIC_GetPendingIRQ(TIMER136_IRQn);
	pendings[89] = NVIC_GetPendingIRQ(TIMER137_IRQn);
	pendings[90] = NVIC_GetPendingIRQ(PWM133_IRQn);
	pendings[91] = NVIC_GetPendingIRQ(SERIAL6_IRQn);
	pendings[92] = NVIC_GetPendingIRQ(SERIAL7_IRQn);
#else
	pendings[0] = NVIC_GetPendingIRQ(SPU000_IRQn);
	pendings[1] = NVIC_GetPendingIRQ(MPC_IRQn);
	pendings[2] = NVIC_GetPendingIRQ(CPUC_IRQn);
	pendings[3] = NVIC_GetPendingIRQ(MVDMA_IRQn);
	pendings[4] = NVIC_GetPendingIRQ(SPU010_IRQn);
	pendings[5] = NVIC_GetPendingIRQ(WDT010_IRQn);
	pendings[6] = NVIC_GetPendingIRQ(WDT011_IRQn);
	pendings[7] = NVIC_GetPendingIRQ(IPCT_0_IRQn);
	pendings[8] = NVIC_GetPendingIRQ(IPCT_1_IRQn);
	pendings[9] = NVIC_GetPendingIRQ(CTI_0_IRQn);
	pendings[10] = NVIC_GetPendingIRQ(CTI_1_IRQn);
	pendings[11] = NVIC_GetPendingIRQ(SWI0_IRQn);
	pendings[12] = NVIC_GetPendingIRQ(SWI1_IRQn);
	pendings[13] = NVIC_GetPendingIRQ(SWI2_IRQn);
	pendings[14] = NVIC_GetPendingIRQ(SWI3_IRQn);
	pendings[15] = NVIC_GetPendingIRQ(SWI4_IRQn);
	pendings[16] = NVIC_GetPendingIRQ(SWI5_IRQn);
	pendings[17] = NVIC_GetPendingIRQ(SWI6_IRQn);
	pendings[18] = NVIC_GetPendingIRQ(SWI7_IRQn);
	pendings[19] = NVIC_GetPendingIRQ(BELLBOARD_0_IRQn);
	pendings[20] = NVIC_GetPendingIRQ(BELLBOARD_1_IRQn);
	pendings[21] = NVIC_GetPendingIRQ(BELLBOARD_2_IRQn);
	pendings[22] = NVIC_GetPendingIRQ(BELLBOARD_3_IRQn);
	pendings[23] = NVIC_GetPendingIRQ(GPIOTE130_0_IRQn);
	pendings[24] = NVIC_GetPendingIRQ(GPIOTE130_1_IRQn);
	pendings[25] = NVIC_GetPendingIRQ(GRTC_0_IRQn);
	pendings[26] = NVIC_GetPendingIRQ(GRTC_1_IRQn);
	pendings[27] = NVIC_GetPendingIRQ(GRTC_2_IRQn);
	pendings[28] = NVIC_GetPendingIRQ(TBM_IRQn);
	pendings[29] = NVIC_GetPendingIRQ(USBHS_IRQn);
	pendings[30] = NVIC_GetPendingIRQ(EXMIF_IRQn);
	pendings[31] = NVIC_GetPendingIRQ(IPCT120_0_IRQn);
	pendings[32] = NVIC_GetPendingIRQ(I3C120_IRQn);
	pendings[33] = NVIC_GetPendingIRQ(VPR121_IRQn);
	pendings[34] = NVIC_GetPendingIRQ(CAN120_IRQn);
	pendings[35] = NVIC_GetPendingIRQ(MVDMA120_IRQn);
	pendings[36] = NVIC_GetPendingIRQ(I3C121_IRQn);
	pendings[37] = NVIC_GetPendingIRQ(TIMER120_IRQn);
	pendings[38] = NVIC_GetPendingIRQ(TIMER121_IRQn);
	pendings[39] = NVIC_GetPendingIRQ(PWM120_IRQn);
	pendings[40] = NVIC_GetPendingIRQ(SPIS120_IRQn);
	pendings[41] = NVIC_GetPendingIRQ(SPIM120_UARTE120_IRQn);
	pendings[42] = NVIC_GetPendingIRQ(SPIM121_IRQn);
	pendings[43] = NVIC_GetPendingIRQ(VPR130_IRQn);
	pendings[44] = NVIC_GetPendingIRQ(IPCT130_0_IRQn);
	pendings[45] = NVIC_GetPendingIRQ(RTC130_IRQn);
	pendings[46] = NVIC_GetPendingIRQ(RTC131_IRQn);
	pendings[47] = NVIC_GetPendingIRQ(WDT131_IRQn);
	pendings[48] = NVIC_GetPendingIRQ(WDT132_IRQn);
	pendings[49] = NVIC_GetPendingIRQ(EGU130_IRQn);
	pendings[50] = NVIC_GetPendingIRQ(SAADC_IRQn);
	pendings[51] = NVIC_GetPendingIRQ(COMP_LPCOMP_IRQn);
	pendings[52] = NVIC_GetPendingIRQ(TEMP_IRQn);
	pendings[53] = NVIC_GetPendingIRQ(NFCT_IRQn);
	pendings[54] = NVIC_GetPendingIRQ(TDM130_IRQn);
	pendings[55] = NVIC_GetPendingIRQ(PDM_IRQn);
	pendings[56] = NVIC_GetPendingIRQ(QDEC130_IRQn);
	pendings[57] = NVIC_GetPendingIRQ(QDEC131_IRQn);
	pendings[58] = NVIC_GetPendingIRQ(TDM131_IRQn);
	pendings[59] = NVIC_GetPendingIRQ(TIMER130_IRQn);
	pendings[60] = NVIC_GetPendingIRQ(TIMER131_IRQn);
	pendings[61] = NVIC_GetPendingIRQ(PWM130_IRQn);
	pendings[62] = NVIC_GetPendingIRQ(SERIAL0_IRQn);
	pendings[63] = NVIC_GetPendingIRQ(SERIAL1_IRQn);
	pendings[64] = NVIC_GetPendingIRQ(TIMER132_IRQn);
	pendings[65] = NVIC_GetPendingIRQ(TIMER133_IRQn);
	pendings[66] = NVIC_GetPendingIRQ(PWM131_IRQn);
	pendings[67] = NVIC_GetPendingIRQ(SERIAL2_IRQn);
	pendings[68] = NVIC_GetPendingIRQ(SERIAL3_IRQn);
	pendings[69] = NVIC_GetPendingIRQ(TIMER134_IRQn);
	pendings[70] = NVIC_GetPendingIRQ(TIMER135_IRQn);
	pendings[71] = NVIC_GetPendingIRQ(PWM132_IRQn);
	pendings[72] = NVIC_GetPendingIRQ(SERIAL4_IRQn);
	pendings[73] = NVIC_GetPendingIRQ(SERIAL5_IRQn);
	pendings[74] = NVIC_GetPendingIRQ(TIMER136_IRQn);
	pendings[75] = NVIC_GetPendingIRQ(TIMER137_IRQn);
	pendings[76] = NVIC_GetPendingIRQ(PWM133_IRQn);
	pendings[77] = NVIC_GetPendingIRQ(SERIAL6_IRQn);
	pendings[78] = NVIC_GetPendingIRQ(SERIAL7_IRQn);
#endif

	__set_BASEPRI(0);
	__ISB();
	__DSB();
	__WFI();
}

static void s2idle_exit(uint8_t substate_id)
{
	int b, i;

#if defined(NRF_RADIOCORE)
	printk("Radio core\n");
#else
	printk("Application core\n");
#endif
	b = 0;
	for (i = 0; i < ARRAY_SIZE(pendings); i++)
	{
		if (pendings[i])
			b++;
		printk("pending[%u] = %u\n", i, pendings[i]);
	}
	printk("Nombre pending : %u.\n", b);

	printk("s2idle_exit substate_id=%u\n", substate_id);

	switch (substate_id) {
	case 0:
		/* Substate for idle with cache powered on - not implemented yet. */
		break;
	case 1: /* Substate for idle with cache retained - not implemented yet. */
		break;
	case 2: /* Substate for idle with cache disabled. */
		common_resume();
#if !defined(CONFIG_SOC_NRF54H20_CPURAD)
		soc_lrcconf_poweron_release(&soc_node, NRF_LRCCONF_POWER_MAIN);
#endif
	default: /* Unknown substate. */
		return;
	}
}

#if defined(CONFIG_PM_S2RAM)
/* Resume domain after local suspend to RAM. */
static void s2ram_exit(void)
{
	common_resume();
#if !defined(CONFIG_SOC_NRF54H20_CPURAD)
	/* Re-enable domain retention. */
	nrf_lrcconf_retain_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_DOMAIN_0, true);
#endif
}

/* Function called during local domain suspend to RAM. */
static int sys_suspend_to_ram(void)
{
	/* Set intormation which is used on domain wakeup to determine if resume from RAM shall
	 * be performed.
	 */
	nrf_resetinfo_resetreas_local_set(NRF_RESETINFO,
					  NRF_RESETINFO_RESETREAS_LOCAL_UNRETAINED_MASK);
	nrf_resetinfo_restore_valid_set(NRF_RESETINFO, true);

#if !defined(CONFIG_SOC_NRF54H20_CPURAD)
	/* Disable retention */
	nrf_lrcconf_retain_set(NRF_LRCCONF010, NRF_LRCCONF_POWER_DOMAIN_0, false);
#endif
	common_suspend();

	__set_BASEPRI(0);
	__ISB();
	__DSB();
	__WFI();
	/*
	 * We might reach this point is k_cpu_idle returns (there is a pre sleep hook that
	 * can abort sleeping.
	 */
	return -EBUSY;
}

static void s2ram_enter(void)
{
	/*
	 * Save the CPU context (including the return address),set the SRAM
	 * marker and power off the system.
	 */
	if (soc_s2ram_suspend(sys_suspend_to_ram)) {
		return;
	}
}
#endif /* defined(CONFIG_PM_S2RAM) */

void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	if (state == PM_STATE_SUSPEND_TO_IDLE) {
		__disable_irq();
		s2idle_enter(substate_id);
		/* Resume here. */
		s2idle_exit(substate_id);
		__enable_irq();
	}
#if defined(CONFIG_PM_S2RAM)
	else if (state == PM_STATE_SUSPEND_TO_RAM) {
		__disable_irq();
		s2ram_enter();
		/* On resuming or error we return exactly *HERE* */
		s2ram_exit();
		__enable_irq();
	}
#endif
	else {
		k_cpu_idle();
	}
}

void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	irq_unlock(0);
}
