//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "hal/board_type.h"

using namespace board_type;

gpio_high_low_e Rascsi_Board_Struct::bool_to_gpio_state(bool value)
{
    if (value) {
        return gpio_high_low_e::GPIO_STATE_HIGH;
    } else {
        return gpio_high_low_e::GPIO_STATE_LOW;
    }
}
bool Rascsi_Board_Struct::gpio_state_to_bool(gpio_high_low_e value)
{
    return (value == gpio_high_low_e::GPIO_STATE_HIGH);
}

// Activity signal true (on)
gpio_high_low_e Rascsi_Board_Struct::ActOn()
{
    return act_on;
}

// Activity signal false (off)
gpio_high_low_e Rascsi_Board_Struct::ActOff()
{
    return GpioInvert(act_on);
}

// Enable signal true (on)
gpio_high_low_e Rascsi_Board_Struct::EnbOn()
{
    return enb_on;
}

// Enable signal false (off)
gpio_high_low_e Rascsi_Board_Struct::EnbOff()
{
    return GpioInvert(enb_on);
}

// Initiator signal = Input
gpio_high_low_e Rascsi_Board_Struct::IndIn()
{
    return ind_in;
}

// Initiator signal = Output
gpio_high_low_e Rascsi_Board_Struct::IndOut()
{
    return GpioInvert(ind_in);
}

// Target signal = Input
gpio_high_low_e Rascsi_Board_Struct::TadIn()
{
    return tad_in;
}

// Target signal = Output
gpio_high_low_e Rascsi_Board_Struct::TadOut()
{
    return GpioInvert(tad_in);
}

// Data signal = Input
gpio_high_low_e Rascsi_Board_Struct::DtdIn()
{
    return dtd_in;
}

// Data signal = Output
gpio_high_low_e Rascsi_Board_Struct::DtdOut()
{
    return GpioInvert(dtd_in);
}

gpio_high_low_e Rascsi_Board_Struct::GpioInvert(gpio_high_low_e in)
{
    if (in == gpio_high_low_e::GPIO_STATE_HIGH) {
        return gpio_high_low_e::GPIO_STATE_LOW;
    } else {
        return gpio_high_low_e::GPIO_STATE_HIGH;
    }
}
