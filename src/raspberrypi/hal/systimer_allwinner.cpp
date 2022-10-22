//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//  Copyright (C) 2022 akuker
//
//	[ High resolution timer for the Allwinner series of SoC's]
//
//---------------------------------------------------------------------------

#include "hal/systimer_allwinner.h"
#include <sys/mman.h>

#include "hal/gpiobus.h"
#include "os.h"

#include "config.h"
#include "log.h"

const std::string SysTimer_AllWinner::dev_mem_filename = "/dev/mem";

//---------------------------------------------------------------------------
//
//	Initialize the system timer
//
//---------------------------------------------------------------------------
void SysTimer_AllWinner::Init()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    int fd;

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
        printf("I can't open /dev/mem. Are you running as root?\n");
        exit(-1);
    }

    hsitimer_regs = (struct sun8i_hsitimer_registers *)mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                                            hs_timer_base_address);

    if (hsitimer_regs == MAP_FAILED) {
        LOGERROR("Unable to map high speed timer registers. Are you running as root?");
    }

    sysbus_regs = (struct sun8i_sysbus_registers *)mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                                                        system_bus_base_address);

    if (sysbus_regs == MAP_FAILED) {
        LOGERROR("Unable to map system bus registers. Are you running as root?");
    }

    enable_hs_timer();
}

void SysTimer_AllWinner::enable_hs_timer()
{
    // By default, the HSTimer clock gating is masked. When it is necessary to use
    // the HSTimer, its clock gating should be opened in BUS Clock Gating Register 0
    // and then de-assert the software reset in BUS Software Reset Register 0 on the
    // CCU module. If it is not needed to use the HSTimer, both the gating bit and
    // the software reset bit should be set 0.

    LOGTRACE("%s [Before Enable] CLK GATE: %08X  SOFT RST: %08X", __PRETTY_FUNCTION__, sysbus_regs->bus_clk_gating_reg0,
             sysbus_regs->bus_soft_rst_reg0);

    sysbus_regs->bus_clk_gating_reg0 = sysbus_regs->bus_clk_gating_reg0 | (1 << BUS_CLK_GATING_REG0_HSTMR);
    sysbus_regs->bus_soft_rst_reg0   = sysbus_regs->bus_soft_rst_reg0 | (1 << BUS_SOFT_RST_REG0_HSTMR);
    LOGTRACE("%s [After Enable] CLK GATE: %08X  SOFT RST: %08X", __PRETTY_FUNCTION__, sysbus_regs->bus_clk_gating_reg0,
             sysbus_regs->bus_soft_rst_reg0);

    // Set interval value to the maximum value. (its a 52 bit register)
    hsitimer_regs->hs_tmr_intv_hi_reg = (1 << 20) - 1; //(0xFFFFF)
    hsitimer_regs->hs_tmr_intv_lo_reg = UINT32_MAX;

    // Select prescale value of 1, continuouse mode
    hsitimer_regs->hs_tmr_ctrl_reg = HS_TMR_CLK_PRE_SCALE_1;

    // Set reload bit
    hsitimer_regs->hs_tmr_ctrl_reg |= HS_TMR_RELOAD;

    // Enable HSTimer
    hsitimer_regs->hs_tmr_ctrl_reg |= HS_TMR_EN;
}

// TODO: According to the data sheet, we should turn off the HS timer when we're done with it. But, its just going to
// eat up a little extra power if we leave it running.
void SysTimer_AllWinner::disable_hs_timer()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    LOGINFO("[Before Disable] CLK GATE: %08X  SOFT RST: %08X", sysbus_regs->bus_clk_gating_reg0,
            sysbus_regs->bus_soft_rst_reg0);

    sysbus_regs->bus_clk_gating_reg0 = sysbus_regs->bus_clk_gating_reg0 & ~(1 << BUS_CLK_GATING_REG0_HSTMR);
    sysbus_regs->bus_soft_rst_reg0   = sysbus_regs->bus_soft_rst_reg0 & ~(1 << BUS_SOFT_RST_REG0_HSTMR);

    LOGINFO("[After Disable] CLK GATE: %08X  SOFT RST: %08X", sysbus_regs->bus_clk_gating_reg0,
            sysbus_regs->bus_soft_rst_reg0);
}

DWORD SysTimer_AllWinner::GetTimerLow()
{
    // RaSCSI expects the timer to count UP, but the Allwinner HS timer counts
    // down. So, we subtract the current timer value from UINT32_MAX
    return (uint32_t)UINT32_MAX - (hsitimer_regs->hs_tmr_curnt_lo_reg / 200);
}

DWORD SysTimer_AllWinner::GetTimerHigh()
{
    return (uint32_t)0;
}

//---------------------------------------------------------------------------
//
//	Sleep in nanoseconds
//
//---------------------------------------------------------------------------
void SysTimer_AllWinner::SleepNsec(DWORD nsec)
{
    // If time is less than one HS timer clock tick, don't do anything
    if (nsec < 20) {
        return;
    }

    // The HS timer receives a 200MHz clock input, which equates to
    // one clock tick every 5 ns.
    uint32_t clockticks = (uint32_t)std::ceil(nsec / 5);

    DWORD enter_time = hsitimer_regs->hs_tmr_curnt_lo_reg;

    LOGTRACE("%s entertime: %08X ns: %d clockticks: %d", __PRETTY_FUNCTION__, enter_time, nsec, clockticks);
    while ((enter_time - hsitimer_regs->hs_tmr_curnt_lo_reg) < clockticks)
        ;

    return;
}

//---------------------------------------------------------------------------
//
//	Sleep in microseconds
//
//---------------------------------------------------------------------------
void SysTimer_AllWinner::SleepUsec(DWORD usec)
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);

    // If time is 0, don't do anything
    if (usec == 0) {
        return;
    }

    DWORD enter_time = GetTimerLow();
    while ((GetTimerLow() - enter_time) < usec)
        ;
}
