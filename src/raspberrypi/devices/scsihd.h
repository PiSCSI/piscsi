//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  Copyright (C) akuker
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ SCSI hard disk ]
//
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "disk.h"
#include "filepath.h"

//===========================================================================
//
//	SCSI Hard Disk
//
//===========================================================================
class SCSIHD : public Disk
{
public:
	// Basic Functions
	SCSIHD();
										// Constructor
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open

	// commands
	int FASTCALL Inquiry(
		const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command
	BOOL FASTCALL ModeSelect(const DWORD *cdb, const BYTE *buf, int length);
										// MODE SELECT(6) command
};