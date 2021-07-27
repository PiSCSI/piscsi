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
//  	[ SCSI Magneto-Optical Disk]
//
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "disk.h"
#include "filepath.h"

//===========================================================================
//
//	SCSI magneto-optical disk
//
//===========================================================================
class SCSIMO : public Disk
{
public:
	// Basic Functions
	SCSIMO();									// Constructor
	bool Open(const Filepath& path, bool attn = true);			// Open

	// commands
	int Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);	// INQUIRY command
	bool ModeSelect(const DWORD *cdb, const BYTE *buf, int length);	// MODE SELECT(6) command

	// Internal processing
	int AddVendor(int page, bool change, BYTE *buf);			// Add vendor special page
};
