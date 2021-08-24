//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// An MmcDevice supports SCSI MMC commands (see https://www.t10.org/drafts.htm, MMC-6)
//
//---------------------------------------------------------------------------

#pragma once

class SASIDEV;

class MmcDevice
{
public:

	MmcDevice() {};
	virtual ~MmcDevice() {};

	virtual void ReadToc(SASIDEV *) = 0;
	virtual void GetEventStatusNotification(SASIDEV *) = 0;
};
