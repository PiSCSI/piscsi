//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

//
// PiSCSI standard (SCSI logic, standard pin assignment)
//

const std::string CONNECT_DESC = "STANDARD"; // Startup message

// Select signal control mode
const static int SIGNAL_CONTROL_MODE = 0; // SCSI logical specification

// Control signal pin assignment (-1 means no control)
const static int PIN_ACT = 4;  // ACTIVE
const static int PIN_ENB = 5;  // ENABLE
const static int PIN_IND = -1; // INITIATOR CTRL DIRECTION
const static int PIN_TAD = -1; // TARGET CTRL DIRECTION
const static int PIN_DTD = -1; // DATA DIRECTION

// Control signal output logic
#define ACT_ON ON  // ACTIVE SIGNAL ON
#define ENB_ON ON  // ENABLE SIGNAL ON
#define IND_IN OFF // INITIATOR SIGNAL INPUT
#define TAD_IN OFF // TARGET SIGNAL INPUT
#define DTD_IN ON  // DATA SIGNAL INPUT

// SCSI signal pin assignment
const static int PIN_DT0 = 10; // Data 0
const static int PIN_DT1 = 11; // Data 1
const static int PIN_DT2 = 12; // Data 2
const static int PIN_DT3 = 13; // Data 3
const static int PIN_DT4 = 14; // Data 4
const static int PIN_DT5 = 15; // Data 5
const static int PIN_DT6 = 16; // Data 6
const static int PIN_DT7 = 17; // Data 7
const static int PIN_DP  = 18; // Data parity
const static int PIN_ATN = 19; // ATN
const static int PIN_RST = 20; // RST
const static int PIN_ACK = 21; // ACK
const static int PIN_REQ = 22; // REQ
const static int PIN_MSG = 23; // MSG
const static int PIN_CD  = 24; // CD
const static int PIN_IO  = 25; // IO
const static int PIN_BSY = 26; // BSY
const static int PIN_SEL = 27; // SEL
