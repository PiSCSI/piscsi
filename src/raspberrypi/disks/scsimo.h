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
//  [ SCSI Magneto-Optical Disk]
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
	SCSIMO();
										// Constructor
	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open
#ifndef RASCSI
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
#endif	// RASCSI

	// commands
	int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command
	BOOL FASTCALL ModeSelect(const DWORD *cdb, const BYTE *buf, int length);
										// MODE SELECT(6) command

	// Internal processing
	int FASTCALL AddVendor(int page, BOOL change, BYTE *buf);
										// Add vendor special page
};