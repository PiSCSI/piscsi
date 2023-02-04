//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI for Raspberry Pi
//  Loopback tester utility
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "scsiloop_timer.h"
#include "hal/systimer.h"
#include "scsiloop/scsiloop_cout.h"
#include "shared/log.h"

int ScsiLoop_Timer::RunTimerTest(vector<string> &error_list)
{
    uint32_t timer_test_failures = 0;

    ScsiLoop_Cout::StartTest("hardware timer");

    // Allow +/- 2% tolerance when testing the timers
    double timer_tolerance_percent  = 0.02;
    const uint32_t one_second_in_ns = 1000000;

    //------------------------------------------------------
    // Test SysTimer::GetTimerLow()
    LOGDEBUG("++ Testing SysTimer::GetTimerLow()")
    uint32_t before = SysTimer::GetTimerLow();
    for (int i = 0; i < 10; i++) {
        usleep(100000);
        ScsiLoop_Cout::PrintUpdate();
    }
    uint32_t after = SysTimer::GetTimerLow();

    uint32_t elapsed_nanosecs = after - before;

    LOGDEBUG("Elapsed time: %d %08X", elapsed_nanosecs, elapsed_nanosecs);

    if ((elapsed_nanosecs > (one_second_in_ns * (1.0 + timer_tolerance_percent))) ||
        (elapsed_nanosecs < (one_second_in_ns * (1.0 - timer_tolerance_percent)))) {
        error_list.push_back(fmt::format("SysTimer::GetTimerLow() test: Expected time approx: {}, but actually {}",
                                         one_second_in_ns, elapsed_nanosecs));
        timer_test_failures++;
    } else {
        ScsiLoop_Cout::PrintUpdate();
    }

    //------------------------------------------------------
    // Test SysTimer::SleepUsec()
    LOGDEBUG("++ Testing SysTimer::SleepUsec()")

    uint32_t expected_usec_result = 1000 * 100;
    before                        = SysTimer::GetTimerLow();
    for (int i = 0; i < 100; i++) {
        SysTimer::SleepUsec(1000);
    }
    after            = SysTimer::GetTimerLow();
    elapsed_nanosecs = after - before;
    LOGDEBUG("SysTimer::SleepUsec() Average %d", (uint32_t)(elapsed_nanosecs / 100));

    if ((elapsed_nanosecs > expected_usec_result * (1.0 + timer_tolerance_percent)) ||
        (elapsed_nanosecs < expected_usec_result * (1.0 - timer_tolerance_percent))) {
        error_list.push_back(fmt::format("SysTimer::SleepUsec Test: Expected time approx: {}, but actually {}",
                                         expected_usec_result, elapsed_nanosecs));
        timer_test_failures++;
    } else {
        ScsiLoop_Cout::PrintUpdate();
    }

    //------------------------------------------------------
    // Test SysTimer::SleepNsec()
    LOGDEBUG("++ Testing SysTimer::SleepNsec()")

    before = SysTimer::GetTimerLow();
    SysTimer::SleepNsec(1000000);
    after = SysTimer::GetTimerLow();
    LOGDEBUG("SysTimer::SleepNSec: %d (expected ~1000)", (uint32_t)(after - before));

    elapsed_nanosecs = after - before;
    if ((elapsed_nanosecs > (1000 * (1.0 + timer_tolerance_percent))) ||
        (elapsed_nanosecs < (1000 * (1.0 - timer_tolerance_percent)))) {
        error_list.push_back(
            fmt::format("SysTimer::SleepNsec Test: Expected time approx: 1000, but actually {}", elapsed_nanosecs));
        timer_test_failures++;
    } else {
        ScsiLoop_Cout::PrintUpdate();
    }

    ScsiLoop_Cout::FinishTest("hardware timer", timer_test_failures);
    return timer_test_failures;
}
