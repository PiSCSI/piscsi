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

namespace board_type
{
// RaSCSI standard (SCSI logic, standard pin assignment)
const Rascsi_Board_Type board_definition_standard = {

    .connect_desc = "STANDARD", // Startup message

    // Select signal control mode
    .signal_control_mode = 0, // SCSI positive logic specification

    // Control signal output logic
    .act_on = gpio_high_low_e::GPIO_STATE_HIGH, // ACTIVE SIGNAL ON
    .enb_on = gpio_high_low_e::GPIO_STATE_HIGH, // ENABLE SIGNAL ON
    .ind_in = gpio_high_low_e::GPIO_STATE_LOW,  // INITIATOR SIGNAL INPUT
    .tad_in = gpio_high_low_e::GPIO_STATE_LOW,  // TARGET SIGNAL INPUT
    .dtd_in = gpio_high_low_e::GPIO_STATE_HIGH, // DATA SIGNAL INPUT

    // Control signal pin assignment (-1 means no control)
    .pin_act = pi_physical_pin_e::PI_PHYS_PIN_07,   // RPi GPIO 4
    .pin_enb = pi_physical_pin_e::PI_PHYS_PIN_29,   // RPi GPIO 5
    .pin_ind = pi_physical_pin_e::PI_PHYS_PIN_NONE, // Not applicable
    .pin_tad = pi_physical_pin_e::PI_PHYS_PIN_NONE, // Not applicable
    .pin_dtd = pi_physical_pin_e::PI_PHYS_PIN_NONE, // Not applicable

    // SCSI signal pin assignment
    .pin_dt0 = pi_physical_pin_e::PI_PHYS_PIN_19, // RPi GPIO 10
    .pin_dt1 = pi_physical_pin_e::PI_PHYS_PIN_23, // RPi GPIO 11
    .pin_dt2 = pi_physical_pin_e::PI_PHYS_PIN_32, // RPi GPIO 12
    .pin_dt3 = pi_physical_pin_e::PI_PHYS_PIN_33, // RPi GPIO 13
    .pin_dt4 = pi_physical_pin_e::PI_PHYS_PIN_08, // RPi GPIO 14
    .pin_dt5 = pi_physical_pin_e::PI_PHYS_PIN_10, // RPi GPIO 15
    .pin_dt6 = pi_physical_pin_e::PI_PHYS_PIN_36, // RPi GPIO 16
    .pin_dt7 = pi_physical_pin_e::PI_PHYS_PIN_11, // RPi GPIO 17
    .pin_dp  = pi_physical_pin_e::PI_PHYS_PIN_12, // RPi GPIO 18
    .pin_atn = pi_physical_pin_e::PI_PHYS_PIN_35, // RPi GPIO 19
    .pin_rst = pi_physical_pin_e::PI_PHYS_PIN_38, // RPi GPIO 20
    .pin_ack = pi_physical_pin_e::PI_PHYS_PIN_40, // RPi GPIO 21
    .pin_req = pi_physical_pin_e::PI_PHYS_PIN_15, // RPi GPIO 22
    .pin_msg = pi_physical_pin_e::PI_PHYS_PIN_16, // RPi GPIO 23
    .pin_cd  = pi_physical_pin_e::PI_PHYS_PIN_18, // RPi GPIO 24
    .pin_io  = pi_physical_pin_e::PI_PHYS_PIN_22, // RPi GPIO 25
    .pin_bsy = pi_physical_pin_e::PI_PHYS_PIN_37, // RPi GPIO 26
    .pin_sel = pi_physical_pin_e::PI_PHYS_PIN_13, // RPi GPIO 27
};
} // namespace board_type