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
// RaSCSI Adapter Aibom version
//

#define CONNECT_DESC "AIBOM PRODUCTS version"		// Startup message

// Select signal control mode
#define SIGNAL_CONTROL_MODE 2				// SCSI positive logic specification

// Control signal output logic
#define ACT_ON		TRUE					// ACTIVE SIGNAL ON
#define ENB_ON		TRUE					// ENABLE SIGNAL ON
#define IND_IN		FALSE					// INITIATOR SIGNAL INPUT
#define TAD_IN		FALSE					// TARGET SIGNAL INPUT
#define DTD_IN		FALSE					// DATA SIGNAL INPUT

// Control signal pin assignment (-1 means no control)
#define	PIN_ACT		4						// ACTIVE
#define	PIN_ENB		17						// ENABLE
#define PIN_IND		27						// INITIATOR CTRL DIRECTION
#define PIN_TAD		-1						// TARGET CTRL DIRECTION
#define PIN_DTD		18						// DATA DIRECTION

// SCSI signal pin assignment
#define	PIN_DT0		6						// Data 0
#define	PIN_DT1		12						// Data 1
#define	PIN_DT2		13						// Data 2
#define	PIN_DT3		16						// Data 3
#define	PIN_DT4		19						// Data 4
#define	PIN_DT5		20						// Data 5
#define	PIN_DT6		26						// Data 6
#define	PIN_DT7		21						// Data 7
#define	PIN_DP		5						// Data parity
#define	PIN_ATN		22						// ATN
#define	PIN_RST		25						// RST
#define	PIN_ACK		10						// ACK
#define	PIN_REQ		7						// REQ
#define	PIN_MSG		9						// MSG
#define	PIN_CD		11						// CD
#define	PIN_IO		23						// IO
#define	PIN_BSY		24						// BSY
#define	PIN_SEL		8						// SEL
