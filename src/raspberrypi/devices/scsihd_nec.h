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
//	SCSI hard disk (PC-9801-55 NEC genuine / Anex86 / T98Next)
//
//===========================================================================
class SCSIHD_NEC : public SCSIHD
{
public:
	SCSIHD_NEC();
	~SCSIHD_NEC() {};

	void Open(const Filepath& path) override;

	// Commands
	int Inquiry(const DWORD *cdb, BYTE *buf) override;

	int AddErrorPage(bool change, BYTE *buf) override;
	int AddFormatPage(bool change, BYTE *buf) override;
	int AddDrivePage(bool change, BYTE *buf) override;

private:
	// Geometry data
	int cylinders;
	int heads;
	int sectors;
};
