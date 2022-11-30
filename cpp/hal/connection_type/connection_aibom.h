//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/pi_defs/bpi-m2p.h"
#include <string>

//
// RaSCSI Adapter Aibom version
//

const std::string CONNECT_DESC = "AIBOM PRODUCTS version"; // Startup message

// Select signal control mode
const static int SIGNAL_CONTROL_MODE = 2; // SCSI positive logic specification

// Control signal output logic
#define ACT_ON ON  // ACTIVE SIGNAL ON
#define ENB_ON ON  // ENABLE SIGNAL ON
#define IND_IN OFF // INITIATOR SIGNAL INPUT
#define TAD_IN OFF // TARGET SIGNAL INPUT
#define DTD_IN OFF // DATA SIGNAL INPUT

// Control signal pin assignment (-1 means no control)
const static int PIN_ACT = 4;  // ACTIVE
const static int PIN_ENB = 17; // ENABLE
const static int PIN_IND = 27; // INITIATOR CTRL DIRECTION
const static int PIN_TAD = -1; // TARGET CTRL DIRECTION
const static int PIN_DTD = 18; // DATA DIRECTION

// SCSI signal pin assignment
const static int PIN_DT0 = 6;  // Data 0
const static int PIN_DT1 = 12; // Data 1
const static int PIN_DT2 = 13; // Data 2
const static int PIN_DT3 = 16; // Data 3
const static int PIN_DT4 = 19; // Data 4
const static int PIN_DT5 = 20; // Data 5
const static int PIN_DT6 = 26; // Data 6
const static int PIN_DT7 = 21; // Data 7
const static int PIN_DP  = 5;  // Data parity
const static int PIN_ATN = 22; // ATN
const static int PIN_RST = 25; // RST
const static int PIN_ACK = 10; // ACK
const static int PIN_REQ = 7;  // REQ
const static int PIN_MSG = 9;  // MSG
const static int PIN_CD  = 11; // CD
const static int PIN_IO  = 23; // IO
const static int PIN_BSY = 24; // BSY
const static int PIN_SEL = 8;  // SEL

// Warning: The Allwinner/Banana Pi GPIO numbers DO NOT correspond to
// the Raspberry Pi GPIO numbers.
//    For example, Pin 7 is GPIO4 on a Raspberry Pi. Its GPIO 6 on a Banana Pi
// For Banana Pi, the pins are specified by physical pin number, NOT GPIO number
// (The Macro's convert the pin number to logical BPi GPIO number)
const static int BPI_PIN_ACT = BPI_M2P_07; // ACTIVE
const static int BPI_PIN_ENB = BPI_M2P_11; // ENABLE
const static int BPI_PIN_IND = BPI_M2P_13; // INITIATOR CTRL DIRECTION
const static int BPI_PIN_TAD = -1;         // TARGET CTRL DIRECTION
const static int BPI_PIN_DTD = BPI_M2P_12; // DATA DIRECTION

const static int BPI_PIN_DT0 = BPI_M2P_31; // Data 0
const static int BPI_PIN_DT1 = BPI_M2P_32; // Data 1
const static int BPI_PIN_DT2 = BPI_M2P_33; // Data 2
const static int BPI_PIN_DT3 = BPI_M2P_36; // Data 3
const static int BPI_PIN_DT4 = BPI_M2P_35; // Data 4
const static int BPI_PIN_DT5 = BPI_M2P_38; // Data 5
const static int BPI_PIN_DT6 = BPI_M2P_37; // Data 6
const static int BPI_PIN_DT7 = BPI_M2P_40; // Data 7
const static int BPI_PIN_DP  = BPI_M2P_29; // Data parity
const static int BPI_PIN_ATN = BPI_M2P_15; // ATN
const static int BPI_PIN_RST = BPI_M2P_22; // RST
const static int BPI_PIN_ACK = BPI_M2P_19; // ACK
const static int BPI_PIN_REQ = BPI_M2P_26; // REQ
const static int BPI_PIN_MSG = BPI_M2P_21; // MSG
const static int BPI_PIN_CD  = BPI_M2P_23; // CD
const static int BPI_PIN_IO  = BPI_M2P_16; // IO
const static int BPI_PIN_BSY = BPI_M2P_18; // BSY
const static int BPI_PIN_SEL = BPI_M2P_24; // SEL