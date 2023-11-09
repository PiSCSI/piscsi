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
// RaSCSI Adapter GAMERnium.com version
//

const std::string CONNECT_DESC = "GAMERnium.com version"; // Startup message

// Select signal control mode
const static int SIGNAL_CONTROL_MODE = 0; // SCSI logical specification

// Control signal output logic
#define ACT_ON ON  // ACTIVE SIGNAL ON
#define ENB_ON ON  // ENABLE SIGNAL ON
#define IND_IN OFF // INITIATOR SIGNAL INPUT
#define TAD_IN OFF // TARGET SIGNAL INPUT
#define DTD_IN ON  // DATA SIGNAL INPUT

// Control signal pin assignment (-1 means no control)
const static int PIN_ACT = 14; // ACTIVE
const static int PIN_ENB = 6;  // ENABLE
const static int PIN_IND = 7;  // INITIATOR CTRL DIRECTION
const static int PIN_TAD = 8;  // TARGET CTRL DIRECTION
const static int PIN_DTD = 5;  // DATA DIRECTION

// SCSI signal pin assignment
const static int PIN_DT0 = 21; // Data 0
const static int PIN_DT1 = 26; // Data 1
const static int PIN_DT2 = 20; // Data 2
const static int PIN_DT3 = 19; // Data 3
const static int PIN_DT4 = 16; // Data 4
const static int PIN_DT5 = 13; // Data 5
const static int PIN_DT6 = 12; // Data 6
const static int PIN_DT7 = 11; // Data 7
const static int PIN_DP  = 25; // Data parity
const static int PIN_ATN = 10; // ATN
const static int PIN_RST = 22; // RST
const static int PIN_ACK = 24; // ACK
const static int PIN_REQ = 15; // REQ
const static int PIN_MSG = 17; // MSG
const static int PIN_CD  = 18; // CD
const static int PIN_IO  = 4;  // IO
const static int PIN_BSY = 27; // BSY
const static int PIN_SEL = 23; // SEL
