//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//	[ Logical representation of a single data sample ]
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/data_sample.h"
#include "shared/scsi.h"

#if defined CONNECT_TYPE_STANDARD
#include "hal/connection_type/connection_standard.h"
#elif defined CONNECT_TYPE_FULLSPEC
#include "hal/connection_type/connection_fullspec.h"
#elif defined CONNECT_TYPE_AIBOM
#include "hal/connection_type/connection_aibom.h"
#elif defined CONNECT_TYPE_GAMERNIUM
#include "hal/connection_type/connection_gamernium.h"
#else
#error Invalid connection type or none specified
#endif

class DataSample_Raspberry final : public DataSample
{
  public:
    bool GetSignal(int pin) const override
    {
        return (bool)((data >> pin) & 1);
    };

    bool GetBSY() const override
    {
        return GetSignal(PIN_BSY);
    }
    bool GetSEL() const override
    {
        return GetSignal(PIN_SEL);
    }
    bool GetATN() const override
    {
        return GetSignal(PIN_ATN);
    }
    bool GetACK() const override
    {
        return GetSignal(PIN_ACK);
    }
    bool GetRST() const override
    {
        return GetSignal(PIN_RST);
    }
    bool GetMSG() const override
    {
        return GetSignal(PIN_MSG);
    }
    bool GetCD() const override
    {
        return GetSignal(PIN_CD);
    }
    bool GetIO() const override
    {
        return GetSignal(PIN_IO);
    }
    bool GetREQ() const override
    {
        return GetSignal(PIN_REQ);
    }
    bool GetACT() const override
    {
        return GetSignal(PIN_ACT);
    }
    bool GetDP() const override
    {
        return GetSignal(PIN_DP);
    }
    uint8_t GetDAT() const override
    {
        return (uint8_t)((data >> (PIN_DT0 - 0)) & (1 << 0)) | ((data >> (PIN_DT1 - 1)) & (1 << 1)) |
               ((data >> (PIN_DT2 - 2)) & (1 << 2)) | ((data >> (PIN_DT3 - 3)) & (1 << 3)) |
               ((data >> (PIN_DT4 - 4)) & (1 << 4)) | ((data >> (PIN_DT5 - 5)) & (1 << 5)) |
               ((data >> (PIN_DT6 - 6)) & (1 << 6)) | ((data >> (PIN_DT7 - 7)) & (1 << 7));
    }

    uint32_t GetRawCapture() const override
    {
        return data;
    }

    DataSample_Raspberry(uint32_t in_data, uint64_t in_timestamp) : DataSample{in_timestamp}, data{in_data} {}
    DataSample_Raspberry() = default;

    ~DataSample_Raspberry() override = default;

  private:
    uint32_t data = 0;
};