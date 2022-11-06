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

#pragma once

#include <string>
#include <memory>

using namespace std;

namespace board_type
{

// This class will define the _physical_ pin numbers associated with the
// GPIOs on the Pi SBC. The equivalent Raspberry Pi GPIO numbers are
// shown as a reference. However, these are NOT applicable to other types
// of SBCs, such as the Banana Pi.
enum class pi_physical_pin_e : int {
    PI_PHYS_PIN_NONE = -1,
    PI_PHYS_PIN_03   = 3,  // RPi GPIO 02
    PI_PHYS_PIN_05   = 5,  // RPi GPIO 03
    PI_PHYS_PIN_07   = 7,  // RPi GPIO 04
    PI_PHYS_PIN_08   = 8,  // RPi GPIO 14
    PI_PHYS_PIN_10   = 10, // RPi GPIO 15
    PI_PHYS_PIN_11   = 11, // RPi GPIO 17
    PI_PHYS_PIN_12   = 12, // RPi GPIO 18
    PI_PHYS_PIN_13   = 13, // RPi GPIO 27
    PI_PHYS_PIN_15   = 15, // RPi GPIO 22
    PI_PHYS_PIN_16   = 16, // RPi GPIO 23
    PI_PHYS_PIN_18   = 18, // RPi GPIO 24
    PI_PHYS_PIN_19   = 19, // RPi GPIO 10
    PI_PHYS_PIN_21   = 21, // RPi GPIO 09
    PI_PHYS_PIN_22   = 22, // RPi GPIO 25
    PI_PHYS_PIN_23   = 23, // RPi GPIO 11
    PI_PHYS_PIN_24   = 24, // RPi GPIO 08
    PI_PHYS_PIN_26   = 26, // RPi GPIO 07
    PI_PHYS_PIN_27   = 27, // RPi GPIO 00
    PI_PHYS_PIN_28   = 28, // RPi GPIO 01
    PI_PHYS_PIN_29   = 29, // RPi GPIO 05
    PI_PHYS_PIN_31   = 31, // RPi GPIO 06
    PI_PHYS_PIN_32   = 32, // RPi GPIO 12
    PI_PHYS_PIN_33   = 33, // RPi GPIO 13
    PI_PHYS_PIN_35   = 35, // RPi GPIO 19
    PI_PHYS_PIN_36   = 36, // RPi GPIO 16
    PI_PHYS_PIN_37   = 37, // RPi GPIO 26
    PI_PHYS_PIN_38   = 38, // RPi GPIO 20
    PI_PHYS_PIN_40   = 40, // RPi GPIO 21
};

// Operation modes definition
enum class gpio_direction_e : uint8_t {
    GPIO_INPUT  = 0,
    GPIO_OUTPUT = 1,
};

enum class gpio_pull_up_down_e : uint8_t {
    GPIO_PULLNONE = 0,
    GPIO_PULLDOWN = 1,
    GPIO_PULLUP   = 2,
};

enum class gpio_high_low_e : uint8_t {
    GPIO_STATE_HIGH = 1, // Equivalent of "ON" in old code
    GPIO_STATE_LOW  = 0, // Equivalent of "OFF" in old code
};

struct Rascsi_Board_Struct {
  public:
    const std::string connect_desc;

    const int signal_control_mode;

    // Control signal output logic
    const gpio_high_low_e act_on; // ACTIVE SIGNAL ON
    const gpio_high_low_e enb_on; // ENABLE SIGNAL ON
    const gpio_high_low_e ind_in; // INITIATOR SIGNAL INPUT
    const gpio_high_low_e tad_in; // TARGET SIGNAL INPUT
    const gpio_high_low_e dtd_in; // DATA SIGNAL INPUT

    // Control signal pin assignment (-1 means no control)
    const pi_physical_pin_e pin_act; // ACTIVE
    const pi_physical_pin_e pin_enb; // ENABLE
    const pi_physical_pin_e pin_ind; // INITIATOR CTRL DIRECTION
    const pi_physical_pin_e pin_tad; // TARGET CTRL DIRECTION
    const pi_physical_pin_e pin_dtd; // DATA DIRECTION

    // SCSI signal pin assignment
    const pi_physical_pin_e pin_dt0; // Data 0
    const pi_physical_pin_e pin_dt1; // Data 1
    const pi_physical_pin_e pin_dt2; // Data 2
    const pi_physical_pin_e pin_dt3; // Data 3
    const pi_physical_pin_e pin_dt4; // Data 4
    const pi_physical_pin_e pin_dt5; // Data 5
    const pi_physical_pin_e pin_dt6; // Data 6
    const pi_physical_pin_e pin_dt7; // Data 7
    const pi_physical_pin_e pin_dp;  // Data parity
    const pi_physical_pin_e pin_atn; // ATN
    const pi_physical_pin_e pin_rst; // RST
    const pi_physical_pin_e pin_ack; // ACK
    const pi_physical_pin_e pin_req; // REQ
    const pi_physical_pin_e pin_msg; // MSG
    const pi_physical_pin_e pin_cd;  // CD
    const pi_physical_pin_e pin_io;  // IO
    const pi_physical_pin_e pin_bsy; // BSY
    const pi_physical_pin_e pin_sel; // SEL

    gpio_high_low_e bool_to_gpio_state(bool);
    bool gpio_state_to_bool(gpio_high_low_e);

    // Activity signal true (on)
    gpio_high_low_e ActOn();
    // Activity signal false (off)
    gpio_high_low_e ActOff();
    // Enable signal true (on)
    gpio_high_low_e EnbOn();
    // Enable signal false (off)
    gpio_high_low_e EnbOff();
    // Initiator signal = Input
    gpio_high_low_e IndIn();
    // Initiator signal = Output
    gpio_high_low_e IndOut();
    // Target signal = Input
    gpio_high_low_e TadIn();
    // Target signal = Output
    gpio_high_low_e TadOut();
    // Data signal = Input
    gpio_high_low_e DtdIn();
    // Data signal = Output
    gpio_high_low_e DtdOut();

    gpio_high_low_e GpioInvert(gpio_high_low_e);
};

typedef struct Rascsi_Board_Struct Rascsi_Board_Type;

// Connection types
enum class rascsi_board_type_e : int {
    BOARD_TYPE_NOT_SPECIFIED = -2,
    BOARD_TYPE_INVALID       = -1,
    BOARD_TYPE_VIRTUAL       = 0,
    BOARD_TYPE_AIBOM         = 1,
    BOARD_TYPE_FULLSPEC      = 2,
    BOARD_TYPE_GAMERNIUM     = 3,
    BOARD_TYPE_STANDARD      = 4,
};

extern const Rascsi_Board_Type board_definition_aibom;
extern const Rascsi_Board_Type board_definition_fullspec;
extern const Rascsi_Board_Type board_definition_gamernium;
extern const Rascsi_Board_Type board_definition_standard;
class BoardType
{
  public:
    static rascsi_board_type_e ParseConnectionType(char *conn);
    static shared_ptr<Rascsi_Board_Type> GetRascsiBoardSettings(rascsi_board_type_e);
};

} // namespace board_type