//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2022 akuker
//
//	[ SCSI Bus Monitor ]
//
//---------------------------------------------------------------------------

#include "data_sample_bananam2p.h"
#include "hal/sunxi_utils.h"
#include <cstdint>

uint8_t DataSample_BananaM2p::GetDAT() const
{
    uint8_t ret_val = 0;
    ret_val |= GetSignal(BPI_PIN_DT0) ? 0x01 : 0x00; // NOSONAR: GCC 10 doesn't fully support std::byte
    ret_val |= GetSignal(BPI_PIN_DT1) ? 0x02 : 0x00; // NOSONAR: GCC 10 doesn't fully support std::byte
    ret_val |= GetSignal(BPI_PIN_DT2) ? 0x04 : 0x00; // NOSONAR: GCC 10 doesn't fully support std::byte
    ret_val |= GetSignal(BPI_PIN_DT3) ? 0x08 : 0x00; // NOSONAR: GCC 10 doesn't fully support std::byte
    ret_val |= GetSignal(BPI_PIN_DT4) ? 0x10 : 0x00; // NOSONAR: GCC 10 doesn't fully support std::byte
    ret_val |= GetSignal(BPI_PIN_DT5) ? 0x20 : 0x00; // NOSONAR: GCC 10 doesn't fully support std::byte
    ret_val |= GetSignal(BPI_PIN_DT6) ? 0x40 : 0x00; // NOSONAR: GCC 10 doesn't fully support std::byte
    ret_val |= GetSignal(BPI_PIN_DT7) ? 0x80 : 0x00; // NOSONAR: GCC 10 doesn't fully support std::byte
    return ret_val;
}

bool DataSample_BananaM2p::GetSignal(int pin) const
{
    int bank = SunXI::GPIO_BANK(pin);
    int num  = SunXI::GPIO_NUM(pin);

    return (bool)((data[bank] >> num) & 0x1);
}

// This will return the Banana Pi data in the "Raspberry Pi" data format.
uint32_t DataSample_BananaM2p::GetRawCapture() const
{
    uint32_t rpi_data = 0;

    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_BSY)) << PIN_BSY;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_SEL)) << PIN_SEL;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_ATN)) << PIN_ATN;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_ACK)) << PIN_ACK;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_RST)) << PIN_RST;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_MSG)) << PIN_MSG;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_CD)) << PIN_CD;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_IO)) << PIN_IO;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_REQ)) << PIN_REQ;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_ACT)) << PIN_ACT;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_DP)) << PIN_DP;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_DT0)) << PIN_DT0;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_DT1)) << PIN_DT1;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_DT2)) << PIN_DT2;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_DT3)) << PIN_DT3;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_DT4)) << PIN_DT4;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_DT5)) << PIN_DT5;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_DT6)) << PIN_DT6;
    rpi_data |= ((uint32_t)GetSignal(BPI_PIN_DT7)) << PIN_DT7;

    return rpi_data;
}