//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  	Copyright (C) akuker
//
//  	Licensed under the BSD 3-Clause License. 
//  	See LICENSE file in the project root folder.
//
//  	[ SCSI NEC "Genuine" Hard Disk]
//
//---------------------------------------------------------------------------
#pragma once

#include "scsihd.h"

//===========================================================================
//
//	SCSI hard disk (PC-9801-55 NEC genuine /Anex86/T98Next)
//
//===========================================================================
class SCSIHD_NEC : public SCSIHD
{
public:
	// Basic Functions
	SCSIHD_NEC();							// Constructor
	void Open(const Filepath& path, BOOL attn = TRUE);		// Open

	// commands
	int Inquiry(const DWORD *cdb, BYTE *buf) override;	// INQUIRY command

	// Internal processing
	int AddError(bool change, BYTE *buf) override;		// Add error
	int AddFormat(bool change, BYTE *buf) override;		// Add format
	int AddDrive(bool change, BYTE *buf) override;		// Add drive

private:
	int cylinders;								// Number of cylinders
	int heads;								// Number of heads
	int sectors;								// Number of sectors
	int sectorsize;								// Sector size
	off_t imgoffset;							// Image offset
	off_t imgsize;							// Image size
};
