//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// A device implementing mandatory SCSI primary commands, to be used for subclassing
//
//---------------------------------------------------------------------------

#pragma once

#include "controllers/sasidev_ctrl.h"
#include "interfaces/scsi_primary_commands.h"
#include "device.h"
#include <string>
#include <map>

using namespace std;

class PrimaryDevice: public Device, virtual public ScsiPrimaryCommands
{
public:

	PrimaryDevice(const string);
	virtual ~PrimaryDevice() {}

	virtual bool Dispatch(SCSIDEV *) override;

	void TestUnitReady(SASIDEV *);
	void Inquiry(SASIDEV *);
	void ReportLuns(SASIDEV *);

	bool CheckReady();
	virtual int Inquiry(const DWORD *, BYTE *) = 0;

protected:

	SASIDEV::ctrl_t *ctrl;

private:

	typedef struct _command_t {
		const char* name;
		void (PrimaryDevice::*execute)(SASIDEV *);

		_command_t(const char* _name, void (PrimaryDevice::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	} command_t;
	std::map<ScsiDefs::scsi_command, command_t*> commands;

	void AddCommand(ScsiDefs::scsi_command, const char*, void (PrimaryDevice::*)(SASIDEV *));

};
