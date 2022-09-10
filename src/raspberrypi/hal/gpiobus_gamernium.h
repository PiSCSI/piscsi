//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ GPIO-SCSI bus ]
//
//---------------------------------------------------------------------------

#pragma once

//
// RaSCSI Adapter GAMERnium.com version
//

#define CONNECT_DESC "GAMERnium.com version"// Startup message

// Select signal control mode
#define SIGNAL_CONTROL_MODE 0				// SCSI logical specification

// Control signal output logic
#define ACT_ON		TRUE					// ACTIVE SIGNAL ON
#define ENB_ON		TRUE					// ENABLE SIGNAL ON
#define IND_IN		FALSE					// INITIATOR SIGNAL INPUT
#define TAD_IN		FALSE					// TARGET SIGNAL INPUT
#define DTD_IN		TRUE					// DATA SIGNAL INPUT

// Control signal pin assignment (-1 means no control)
#define	PIN_ACT		14						// ACTIVE
#define	PIN_ENB		6						// ENABLE
#define PIN_IND		7						// INITIATOR CTRL DIRECTION
#define PIN_TAD		8						// TARGET CTRL DIRECTION
#define PIN_DTD		5						// DATA DIRECTION

// SCSI signal pin assignment
#define	PIN_DT0		21						// Data 0
#define	PIN_DT1		26						// Data 1
#define	PIN_DT2		20						// Data 2
#define	PIN_DT3		19						// Data 3
#define	PIN_DT4		16						// Data 4
#define	PIN_DT5		13						// Data 5
#define	PIN_DT6		12						// Data 6
#define	PIN_DT7		11						// Data 7
#define	PIN_DP		25						// Data parity
#define	PIN_ATN		10						// ATN
#define	PIN_RST		22						// RST
#define	PIN_ACK		24						// ACK
#define	PIN_REQ		15						// REQ
#define	PIN_MSG		17						// MSG
#define	PIN_CD		18						// CD
#define	PIN_IO		4						// IO
#define	PIN_BSY		27						// BSY
#define	PIN_SEL		23						// SEL

