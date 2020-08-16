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
//  [ SASI hard disk ]
//
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "disk.h"
#include "filepath.h"

//===========================================================================
//
//	SASI Hard Disk
//
//===========================================================================
class SASIHD : public Disk
{
public:
	// Basic Functions
	SASIHD();
										// Constructor
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open
	// commands
	int FASTCALL RequestSense(const DWORD *cdb, BYTE *buf);
										// REQUEST SENSE command
};