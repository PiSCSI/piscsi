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
//  [ SCSI Hard Disk for Apple Macintosh ]
//
//---------------------------------------------------------------------------
#pragma once

#include "scsihd.h"

//===========================================================================
//
//	SCSI Hard Disk(Genuine Apple Macintosh)
//
//===========================================================================
class SCSIHD_APPLE : public SCSIHD
{
public:
	// Basic Functions
	SCSIHD_APPLE();
										// Constructor
	// commands
	int FASTCALL Inquiry(
		const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command

	// Internal processing
	int FASTCALL AddVendor(int page, BOOL change, BYTE *buf);
										// Add vendor special page
};