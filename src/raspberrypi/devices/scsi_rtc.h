//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "disk.h"
#include "../rascsi.h"

class SCSIRtc: public Disk
{

private:
	typedef struct _command_t {
		const char* name;
		void (SCSIRtc::*execute)(SASIDEV *);

		_command_t(const char* _name, void (SCSIRtc::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	} command_t;
	std::map<SCSIDEV::scsi_command, command_t*> commands;

	SASIDEV::ctrl_t *ctrl;

public:
	SCSIRtc();
	~SCSIRtc();

	virtual bool Dispatch(SCSIDEV *) override;

	int Inquiry(const DWORD *, BYTE *) override;
	void TestUnitReady(SASIDEV *) override;
	int AddVendorPage(int, bool, BYTE *) override;
};
