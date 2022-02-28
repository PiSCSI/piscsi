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
class SASIHD : public Disk, public FileSupport
{
public:
	SASIHD(const set<uint32_t>&);
	~SASIHD() {}

	void Reset();
	void Open(const Filepath&) override;

	// Commands
	int RequestSense(const DWORD *, BYTE *) override;
	vector<BYTE> Inquiry() const override;
};
