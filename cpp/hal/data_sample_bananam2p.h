//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2020-2021 akuker
//
//	[ Logical representation of a single data sample ]
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
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

class DataSample_BananaM2p  : public DataSample
{
  public:

    bool GetSignal(int pin) const override;

    bool GetBSY() const override
    {
        return GetSignal(BPI_PIN_BSY);
    }
    bool GetSEL() const override
    {
        return GetSignal(BPI_PIN_SEL);
    }
    bool GetATN() const override
    {
        return GetSignal(BPI_PIN_ATN);
    }
    bool GetACK() const override
    {
        return GetSignal(BPI_PIN_ACK);
    }
    bool GetRST() const override
    {
        return GetSignal(BPI_PIN_RST);
    }
    bool GetMSG() const override
    {
        return GetSignal(BPI_PIN_MSG);
    }
    bool GetCD() const override
    {
        return GetSignal(BPI_PIN_CD);
    }
    bool GetIO() const override
    {
        return GetSignal(BPI_PIN_IO);
    }
    bool GetREQ() const override
    {
        return GetSignal(BPI_PIN_REQ);
    }
    bool GetACT() const override
    {
        return GetSignal(BPI_PIN_ACT);
    }
    bool GetDP() const override
    {
        return GetSignal(BPI_PIN_DP);
    }

    uint8_t GetDAT() const override;

	uint32_t GetRawCapture() const override;

    	bool operator==(const DataSample& other) const override{ return data == dynamic_cast<const DataSample_BananaM2p*>(&other)->data;}

    DataSample_BananaM2p(array<uint32_t, 12> in_data, uint64_t in_timestamp)
    {
        data      = in_data;
        timestamp = in_timestamp;
    }
    DataSample_BananaM2p() = default;

  private:
    array<uint32_t, 12> data = {0};
};