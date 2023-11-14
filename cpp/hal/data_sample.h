//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//	[ Logical representation of a single data sample ]
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/scsi.h"
#include <cstdint>
#include <string>

using namespace scsi_defs;

class DataSample
{
  public:
    virtual bool GetBSY() const    = 0;
    virtual bool GetSEL() const    = 0;
    virtual bool GetATN() const    = 0;
    virtual bool GetACK() const    = 0;
    virtual bool GetRST() const    = 0;
    virtual bool GetMSG() const    = 0;
    virtual bool GetCD() const     = 0;
    virtual bool GetIO() const     = 0;
    virtual bool GetREQ() const    = 0;
    virtual bool GetACT() const    = 0;
    virtual uint8_t GetDAT() const = 0;

    virtual uint32_t GetRawCapture() const = 0;

    phase_t GetPhase() const;
    virtual bool GetSignal(int pin) const = 0;

    uint64_t GetTimestamp() const
    {
        return timestamp;
    }

    string GetPhaseStr() const;

    virtual ~DataSample() = default;

    explicit DataSample(uint64_t in_timestamp) : timestamp{in_timestamp} {}
    DataSample() = default;

  private:
    uint64_t timestamp = 0;
};
