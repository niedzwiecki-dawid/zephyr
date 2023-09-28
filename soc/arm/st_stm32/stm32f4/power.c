/*
 * Copyright (c) 2021 Fabio Baltieri
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include <soc.h>
#include <zephyr/init.h>
#include <zephyr/drivers/counter.h>

//#include <stm32l0xx_ll_utils.h>
//#include <stm32l0xx_ll_bus.h>
#include <stm32f4xx_ll_cortex.h>
//#include <zephyr/arch/arm/aarch32/cortex_m/cmsis.h>
#include <stm32f4xx_ll_pwr.h>
//#include <stm32l0xx_ll_rcc.h>
//#include <stm32l0xx_ll_system.h>
#include <stm32f4xx.h>
#include <clock_control/clock_stm32_ll_common.h>
#include <zephyr/drivers/clock_control/stm32_clock_control.h>

#include <zephyr/drivers/interrupt_controller/gic.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(soc, CONFIG_SOC_LOG_LEVEL);

/* Select MSI as wake-up system clock if configured, HSI otherwise */
#if STM32_SYSCLK_SRC_MSI
#define RCC_STOP_WAKEUPCLOCK_SELECTED LL_RCC_STOP_WAKEUPCLOCK_MSI
#else
#define RCC_STOP_WAKEUPCLOCK_SELECTED LL_RCC_STOP_WAKEUPCLOCK_HSI
#endif

//uint32_t ticks_old;
//uint32_t ticks_new;
//void rtc_alarm_cb(const struct device *dev, uint8_t chan_id, uint32_t ticks, void *user_data) {
////	printk("DN rtc alarm, ticks: %d\n", ticks);
////	printk("DN diff ticks_new: %d\n", ticks_new - ticks_old);
////	printk("DN diff usecs: %llu\n", counter_ticks_to_us(dev, ticks - ticks_old));
////	printk("DN uptime: %lld", k_uptime_get());
//}

static int irq_num_before;
static int irq_num_after;
static int irq_tab_before[10];
static int irq_tab_after[10];

/* Invoke Low Power/System Off specific Tasks */
__weak void pm_state_set(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);
//	uint32_t rtc_ticks = 0;

//	ticks0 = rtc_ticks;
//	k_msleep(1);
//	counter_get_value(rtc, &rtc_ticks);
//	printk("DN rtc tics for 1ms: %d\n", ticks0 - rtc_ticks);

//	printk("DN entering state: %d\n", state);

//	while (1) {
//		counter_get_value(rtc, &rtc_ticks);
//		if (ticks0 != rtc_ticks) {
//			ticks0 = rtc_ticks;
//			printk("DN rtc ticks: %d\n", rtc_ticks);
//			printk("DN rtc us: %lld\n", counter_ticks_to_us(rtc, rtc_ticks));
//		}
//	}

//	printk("DN setting state %d\n", state);
	for(volatile int i = 0; i < 10000; i++){
	}
	switch (state) {
	case PM_STATE_SUSPEND_TO_IDLE:
		LL_PWR_ClearFlag_WU();
		LL_PWR_SetPowerMode(LL_PWR_MODE_STOP_MAINREGU);
		LL_LPM_EnableDeepSleep();
		/* ensure outstanding memory transactions complete */
//		uint32_t basepri, primask;
//		basepri = __get_BASEPRI();
//		primask = __get_PRIMASK();
//
//		printk("DN: base: 0x%08x, pri: 0x%08x \n", basepri, primask);

//		for (int i = 0; i < 256; i++) {
//			if (NVIC_GetPendingIRQ(i)) {
//				printk("DN: irq: %d pending\n", i);
//			}
//		}

//		printk("DN start2\n");

		__asm__ volatile ("cpsid i");
		__asm__ volatile ("eors.n r0, r0");
		__asm__ volatile ("msr BASEPRI, r0");
		__asm__ volatile ("isb");

		__asm__ volatile ("dsb");

//		while(1) {
//			for (int i = 0; i < 256; i++) {
//				if (NVIC_GetPendingIRQ(i)) {
////					if (i != 80)
//					printk("DN: irq: %d pending\n", i);
//				}
//			}
//		}

		irq_num_before = 0;
		for (int i = 0; i < 256; i++) {
			if (NVIC_GetPendingIRQ(i)) {
				irq_tab_before[irq_num_before] = i;
				irq_num_before++;
//					if (i != 80)
//					printk("DN: irq: %d pending after\n", i);
			}
		}

		__asm__ volatile ("wfi");
//
//		printk("DN wakeup\n");
//		__asm__ volatile ("dsb");
//		while(1) {
//		}

		irq_num_after = 0;
		for (int i = 0; i < 256; i++) {
			if (NVIC_GetPendingIRQ(i)) {
				irq_tab_after[irq_num_after] = i;
				irq_num_after++;
			}
		}

		__asm__ volatile ("cpsie i");
		__asm__ volatile ("isb");


//		k_cpu_idle();

//		printk("DN wakeup 3\n");

		break;
	default:
		printk("DN Unsupported power state\n");
		LOG_DBG("Unsupported power state %u", state);
		break;
	}
}

/* Handle SOC specific activity after Low Power Mode Exit */
__weak void pm_state_exit_post_ops(enum pm_state state, uint8_t substate_id)
{
	ARG_UNUSED(substate_id);
//	const struct device *rtc = DEVICE_DT_GET(DT_NODELABEL(rtc));
//	counter_get_value(rtc, &ticks_new);

	switch (state) {
	case PM_STATE_SUSPEND_TO_IDLE:
		LL_LPM_DisableSleepOnExit();
		LL_LPM_EnableSleep();
//		LL_PWR_SetRegulModeLP(LL_PWR_REGU_LPMODES_MAIN);

		/* Restore the clock setup. */
		stm32_clock_control_init(NULL);
		break;
	default:
		printk("Unsupported power substate-id %u\n", state);
		LOG_DBG("Unsupported power substate-id %u", state);
		break;
	}
//	printk("DN exiting state: %d, WU: %d\n", state, LL_PWR_IsActiveFlag_WU());
//	if (irq_num_before > 0) {
//		printk("DN pending irqs before:");
//		for (int i = 0; i < irq_num_before; i++) {
//			printk(" %d", irq_tab_before[i]);
//		}
//		printk("\n");
//	}
//	if (irq_num_after > 0) {
//		printk("DN pending irqs after:");
//		for (int i = 0; i < irq_num_after; i++) {
//			printk(" %d", irq_tab_after[i]);
//		}
//		printk("\n");
//	}

	/*
	 * System is now in active mode. Reenable interrupts which were
	 * disabled when OS started idling code.
	 */
	irq_unlock(0);
}

/* Initialize STM32 Power */
static int stm32_power_init(void)
{
	/* Enable Power clock */
//	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
//
//#ifdef CONFIG_DEBUG
//	/* Enable the Debug Module during STOP mode */
//	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_DBGMCU);
//	LL_DBGMCU_EnableDBGStopMode();
//	LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_DBGMCU);
//#endif /* CONFIG_DEBUG */

	return 0;
}

SYS_INIT(stm32_power_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
