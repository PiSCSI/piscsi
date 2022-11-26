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
#include <unordered_map>
#include <vector>
#include <memory>
using namespace std;

#ifndef USE_SEL_EVENT_ENABLE
#define USE_SEL_EVENT_ENABLE
#endif

class BUS : public PinControl
{
  public:
    // Operation modes definition
    enum class mode_e {
        TARGET    = 0,
        INITIATOR = 1,
        MONITOR   = 2,
    };

    BUS()          = default;
    virtual ~BUS() = default;

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
    virtual int GetMode(int pin)               = 0;

    virtual uint32_t GetPinRaw(uint32_t raw_data, uint32_t pin_num) = 0;

    virtual uint32_t Acquire()                                                = 0;
    virtual unique_ptr<DataSample> GetSample(uint64_t timestamp = 0)          = 0;
    virtual int CommandHandShake(vector<uint8_t> &)                           = 0;
    virtual int ReceiveHandShake(uint8_t *buf, int count)                     = 0;
    virtual int SendHandShake(uint8_t *buf, int count, int delay_after_bytes) = 0;

#ifdef USE_SEL_EVENT_ENABLE
    // SEL signal event polling
    virtual bool PollSelectEvent() = 0;

    // Clear SEL signal event
    virtual void ClearSelectEvent() = 0;
#endif

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
