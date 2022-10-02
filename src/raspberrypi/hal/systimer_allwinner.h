//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//  Copyright (C) 2022 akuker
//
//	[ High resolution timer ]
//
//---------------------------------------------------------------------------
#pragma once

#include <stdint.h>
#include <string>
#include "systimer.h"

//===========================================================================
//
//	System timer
//
//===========================================================================
class SysTimer_AllWinner : public SysTimer
{
public:
	// Initialization
	void Init() override;
	// Get system timer low byte
	uint32_t GetTimerLow() override;
	// Get system timer high byte
	uint32_t GetTimerHigh() override;
	// Sleep for N nanoseconds
	void SleepNsec(uint32_t nsec) override;
	// Sleep for N microseconds
	void SleepUsec(uint32_t usec) override;

private:
	void enable_hs_timer();
	void disable_hs_timer();

	static const std::string dev_mem_filename;

	/* Reference: Allwinner H3 Datasheet, section 4.9.3 */
	static const uint32_t hs_timer_base_address = 0x01C60000;
	/* Note: Currently the high speed timer is NOT in the armbian
	 * device tree. If it is ever added, this should be pulled
	 * from there */

	struct sun8i_hsitimer_registers
	{
		/* 0x00 HS Timer IRQ Enabled Register */
		uint32_t hs_tmr_irq_en_reg;
		/* 0x04 HS Timer Status Register */
		uint32_t hs_tmr_irq_stat_reg;
		/* 0x08 Unused */
		uint32_t unused_08;
		/* 0x0C Unused */
		uint32_t unused_0C;
		/* 0x10 HS Timer Control Register */
		uint32_t hs_tmr_ctrl_reg;
		/* 0x14 HS Timer Interval Value Low Reg */
		uint32_t hs_tmr_intv_lo_reg;
		/* 0x18 HS Timer Interval Value High Register */
		uint32_t hs_tmr_intv_hi_reg;
		/* 0x1C HS Timer Current Value Low Register */
		uint32_t hs_tmr_curnt_lo_reg;
		/* 0x20 HS Timer Current Value High Register */
		uint32_t hs_tmr_curnt_hi_reg;
	};

	/* Constants for the HS Timer IRQ enable Register (section 4.9.4.1) */
	static const uint32_t HS_TMR_INTERUPT_ENB = (1 << 0);

	/* Constants for the HS Timer Control Register (section 4.9.4.3) */
	static const uint32_t HS_TMR_EN = (1 << 0);
	static const uint32_t HS_TMR_RELOAD = (1 << 1);
	static const uint32_t HS_TMR_CLK_PRE_SCALE_1 = (0 << 4);
	static const uint32_t HS_TMR_CLK_PRE_SCALE_2 = (1 << 4);
	static const uint32_t HS_TMR_CLK_PRE_SCALE_4 = (2 << 4);
	static const uint32_t HS_TMR_CLK_PRE_SCALE_8 = (3 << 4);
	static const uint32_t HS_TMR_CLK_PRE_SCALE_16 = (4 << 4);
	static const uint32_t HS_TMR_MODE_SINGLE = (1 << 7);
	static const uint32_t HS_TMR_TEST_MODE = (1 << 31);

	struct sun8i_hsitimer_registers *hsitimer_regs;

	/* Reference: Allwinner H3 Datasheet, section 4.3.4 */
	static const uint32_t system_bus_base_address = 0x01C20000;

	struct sun8i_sysbus_registers
	{
		uint32_t pad_00_5C[(0x60 / sizeof(uint32_t))];
		/* 0x0060 Bus Clock Gating Register 0 */
		uint32_t bus_clk_gating_reg0;
		uint32_t pad_64_2C0[((0x2C0 - 0x64) / sizeof(uint32_t))];
		/* 0x2C0 Bus Software Reset Register 0 */
		uint32_t bus_soft_rst_reg0;
	};

	/* Bit associated with the HS Timer */
	static const uint32_t BUS_CLK_GATING_REG0_HSTMR = 19;
	static const uint32_t BUS_SOFT_RST_REG0_HSTMR = 19;

	struct sun8i_sysbus_registers *sysbus_regs;
};
