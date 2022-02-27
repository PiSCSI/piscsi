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

class SCSIMO : public Disk, public FileSupport
{
public:
	SCSIMO() : Disk("SCMO") {}
	~SCSIMO() {}

	void Open(const Filepath& path) override;

	// Commands
	int Inquiry(const DWORD *cdb, BYTE *buf) override;
	bool ModeSelect(const DWORD *cdb, const BYTE *buf, int length) override;

protected:

	// Internal processing
	void SetDeviceParameters(BYTE *) override;
	void AddModePages(map<int, vector<BYTE>>&, int, bool) const override;
	void AddVendorPage(map<int, vector<BYTE>>&, int, bool) const override;

private:

	void AddOptionPage(map<int, vector<BYTE>>&, bool) const;
};
