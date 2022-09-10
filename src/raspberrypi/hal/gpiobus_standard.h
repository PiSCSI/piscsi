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
// RaSCSI standard (SCSI logic, standard pin assignment)
//
#define CONNECT_DESC "STANDARD"				// Startup message

// Select signal control mode
#define SIGNAL_CONTROL_MODE 0				// SCSI logical specification

// Control signal pin assignment (-1 means no control)
#define	PIN_ACT		4						// ACTIVE
#define	PIN_ENB		5						// ENABLE
#define PIN_IND		-1						// INITIATOR CTRL DIRECTION
#define PIN_TAD		-1						// TARGET CTRL DIRECTION
#define PIN_DTD		-1						// DATA DIRECTION

// Control signal output logic
#define ACT_ON		TRUE					// ACTIVE SIGNAL ON
#define ENB_ON		TRUE					// ENABLE SIGNAL ON
#define IND_IN		FALSE					// INITIATOR SIGNAL INPUT
#define TAD_IN		FALSE					// TARGET SIGNAL INPUT
#define DTD_IN		TRUE					// DATA SIGNAL INPUT

// SCSI signal pin assignment
#define	PIN_DT0		10						// Data 0
#define	PIN_DT1		11						// Data 1
#define	PIN_DT2		12						// Data 2
#define	PIN_DT3		13						// Data 3
#define	PIN_DT4		14						// Data 4
#define	PIN_DT5		15						// Data 5
#define	PIN_DT6		16						// Data 6
#define	PIN_DT7		17						// Data 7
#define	PIN_DP		18						// Data parity
#define	PIN_ATN		19						// ATN
#define	PIN_RST		20						// RST
#define	PIN_ACK		21						// ACK
#define	PIN_REQ		22						// REQ
#define	PIN_MSG		23						// MSG
#define	PIN_CD		24						// CD
#define	PIN_IO		25						// IO
#define	PIN_BSY		26						// BSY
#define	PIN_SEL		27						// SEL


