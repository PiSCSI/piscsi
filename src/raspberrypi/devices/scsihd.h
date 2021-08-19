//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License. 
//	See LICENSE file in the project root folder.
//
//  [ SCSI hard disk ]
//
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "disk.h"
#include "filepath.h"

#define DEFAULT_PRODUCT "SCSI HD"

//===========================================================================
//
//	SCSI Hard Disk
//
//===========================================================================
class SCSIHD : public Disk, public FileSupport
{
public:
	// Basic Functions
	SCSIHD(bool = false);					// Constructor
	void Reset();							// Reset
	void Open(const Filepath& path);		// Open

	// commands
	int Inquiry(const DWORD *cdb, BYTE *buf) override;	// INQUIRY command
	bool ModeSelect(const DWORD *cdb, const BYTE *buf, int length) override;	// MODE SELECT(6) command

	// Internal processing
	int AddVendor(int page, bool change, BYTE *buf) override;	// Add vendor special page
};
