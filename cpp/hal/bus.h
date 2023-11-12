//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/data_sample.h"
#include "hal/pin_control.h"
#include "shared/config.h"
#include "shared/scsi.h"
#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <stdexcept>
using namespace std;

//---------------------------------------------------------------------------
//
//	Constant declarations (bus control timing)
//
//---------------------------------------------------------------------------
// SCSI Bus timings taken from:
//     https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-05.html
const static int SCSI_DELAY_ARBITRATION_DELAY_NS         = 2400;
const static int SCSI_DELAY_ASSERTION_PERIOD_NS          = 90;
const static int SCSI_DELAY_BUS_CLEAR_DELAY_NS           = 800;
const static int SCSI_DELAY_BUS_FREE_DELAY_NS            = 800;
const static int SCSI_DELAY_BUS_SET_DELAY_NS             = 1800;
const static int SCSI_DELAY_BUS_SETTLE_DELAY_NS          = 400;
const static int SCSI_DELAY_CABLE_SKEW_DELAY_NS          = 10;
const static int SCSI_DELAY_DATA_RELEASE_DELAY_NS        = 400;
const static int SCSI_DELAY_DESKEW_DELAY_NS              = 45;
const static int SCSI_DELAY_DISCONNECTION_DELAY_US       = 200;
const static int SCSI_DELAY_HOLD_TIME_NS                 = 45;
const static int SCSI_DELAY_NEGATION_PERIOD_NS           = 90;
const static int SCSI_DELAY_POWER_ON_TO_SELECTION_TIME_S = 10;         // (recommended)
const static int SCSI_DELAY_RESET_TO_SELECTION_TIME_US   = 250 * 1000; // (recommended)
const static int SCSI_DELAY_RESET_HOLD_TIME_US           = 25;
const static int SCSI_DELAY_SELECTION_ABORT_TIME_US      = 200;
const static int SCSI_DELAY_SELECTION_TIMEOUT_DELAY_NS   = 250 * 1000; // (recommended)
const static int SCSI_DELAY_FAST_ASSERTION_PERIOD_NS     = 30;
const static int SCSI_DELAY_FAST_CABLE_SKEW_DELAY_NS     = 5;
const static int SCSI_DELAY_FAST_DESKEW_DELAY_NS         = 20;
const static int SCSI_DELAY_FAST_HOLD_TIME_NS            = 10;
const static int SCSI_DELAY_FAST_NEGATION_PERIOD_NS      = 30;

// The DaynaPort SCSI Link do a short delay in the middle of transfering
// a packet. This is the number of uS that will be delayed between the
// header and the actual data.
const static int SCSI_DELAY_SEND_DATA_DAYNAPORT_US = 100;


class bus_exception : public runtime_error
{
	using runtime_error::runtime_error;
};

class BUS : public PinControl
{
  public:
    // Operation modes definition
    enum class mode_e {
        TARGET    = 0,
        INITIATOR = 1
    };

    static int GetCommandByteCount(uint8_t);

    virtual bool Init(mode_e mode) = 0;
    virtual void Reset()           = 0;
    virtual void Cleanup()         = 0;
    phase_t GetPhase();

    static phase_t GetPhase(int mci)
    {
        return phase_table[mci];
    }

    // Get the string phase name, based upon the raw data
    static const char *GetPhaseStrRaw(phase_t current_phase);

    virtual uint32_t Acquire()                                                = 0;
    virtual unique_ptr<DataSample> GetSample(uint64_t timestamp = 0)          = 0;
    virtual int CommandHandShake(vector<uint8_t> &)                           = 0;
    virtual int ReceiveHandShake(uint8_t *buf, int count)                     = 0;
    virtual int SendHandShake(uint8_t *buf, int count, int delay_after_bytes) = 0;

    // SEL signal event polling
    virtual bool PollSelectEvent() = 0;

    virtual bool GetSignal(int pin) const = 0;
    // Get SCSI input signal value
    virtual void SetSignal(int pin, bool ast) = 0;
    // Set SCSI output signal value
    static const int SEND_NO_DELAY = -1;
    // Passed into SendHandShake when we don't want to delay
  private:
    static const array<phase_t, 8> phase_table;

    static const unordered_map<phase_t, const char *> phase_str_mapping;
};
