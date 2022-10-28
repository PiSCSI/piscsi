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

namespace board_type{
// RaSCSI standard (SCSI logic, standard pin assignment)
const Rascsi_Board_Type board_definition_fullspec = {

    .connect_desc = "FULLSPEC", // Startup message

    // Select signal control mode
    .signal_control_mode = 0, // SCSI positive logic specification

    // Control signal output logic
    .act_on = gpio_high_low_e::GPIO_STATE_HIGH, // ACTIVE SIGNAL ON
    .enb_on = gpio_high_low_e::GPIO_STATE_HIGH, // ENABLE SIGNAL ON
    .ind_in = gpio_high_low_e::GPIO_STATE_LOW,  // INITIATOR SIGNAL INPUT
    .tad_in = gpio_high_low_e::GPIO_STATE_LOW,  // TARGET SIGNAL INPUT
    .dtd_in = gpio_high_low_e::GPIO_STATE_HIGH, // DATA SIGNAL INPUT

    // Control signal pin assignment (-1 means no control)
    .pin_act = pi_physical_pin_e::PI_PHYS_PIN_07, // ACTIVE
    .pin_enb = pi_physical_pin_e::PI_PHYS_PIN_29, // ENABLE
    .pin_ind = pi_physical_pin_e::PI_PHYS_PIN_31, // INITIATOR CTRL DIRECTION
    .pin_tad = pi_physical_pin_e::PI_PHYS_PIN_26, // TARGET CTRL DIRECTION
    .pin_dtd = pi_physical_pin_e::PI_PHYS_PIN_24, // DATA DIRECTION

    // SCSI signal pin assignment
    .pin_dt0 = pi_physical_pin_e::PI_PHYS_PIN_19, // Data 0
    .pin_dt1 = pi_physical_pin_e::PI_PHYS_PIN_23, // Data 1
    .pin_dt2 = pi_physical_pin_e::PI_PHYS_PIN_32, // Data 2
    .pin_dt3 = pi_physical_pin_e::PI_PHYS_PIN_33, // Data 3
    .pin_dt4 = pi_physical_pin_e::PI_PHYS_PIN_08, // Data 4
    .pin_dt5 = pi_physical_pin_e::PI_PHYS_PIN_10, // Data 5
    .pin_dt6 = pi_physical_pin_e::PI_PHYS_PIN_36, // Data 6
    .pin_dt7 = pi_physical_pin_e::PI_PHYS_PIN_11, // Data 7
    .pin_dp  = pi_physical_pin_e::PI_PHYS_PIN_12, // Data parity
    .pin_atn = pi_physical_pin_e::PI_PHYS_PIN_35, // ATN
    .pin_rst = pi_physical_pin_e::PI_PHYS_PIN_38, // RST
    .pin_ack = pi_physical_pin_e::PI_PHYS_PIN_40, // ACK
    .pin_req = pi_physical_pin_e::PI_PHYS_PIN_15, // REQ
    .pin_msg = pi_physical_pin_e::PI_PHYS_PIN_16, // MSG
    .pin_cd  = pi_physical_pin_e::PI_PHYS_PIN_18, // CD
    .pin_io  = pi_physical_pin_e::PI_PHYS_PIN_22, // IO
    .pin_bsy = pi_physical_pin_e::PI_PHYS_PIN_37, // BSY
    .pin_sel = pi_physical_pin_e::PI_PHYS_PIN_13, // SEL
};
}