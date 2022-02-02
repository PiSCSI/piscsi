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

	void AddCommand(SCSIDEV::scsi_command, const char*, void (SCSIRtc::*)(SASIDEV *));

public:
	SCSIRtc();
	~SCSIRtc();

	virtual bool Dispatch(SCSIDEV *) override;

	int Inquiry(const DWORD *cdb, BYTE *buffer) override;
	void TestUnitReady(SASIDEV *) override;
	void ModeSense6(SASIDEV *) override;
	void ModeSense10(SASIDEV *) override;
};
