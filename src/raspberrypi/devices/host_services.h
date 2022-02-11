//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// RaSCSI Host Services with realtime clock and shutdown support
//
//---------------------------------------------------------------------------
#pragma once

#include "mode_page_device.h"
#include <map>

using namespace std;

class HostServices: public ModePageDevice
{
public:

	HostServices();
	~HostServices() {}

	virtual bool Dispatch(SCSIDEV *) override;

	int Inquiry(const DWORD *, BYTE *) override;
	void TestUnitReady(SASIDEV *) override;
	void StartStopUnit(SASIDEV *);

	int ModeSense6(const DWORD *, BYTE *);
	int ModeSense10(const DWORD *, BYTE *);

private:

	typedef struct _command_t {
		const char* name;
		void (HostServices::*execute)(SASIDEV *);

		_command_t(const char* _name, void (HostServices::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	} command_t;
	std::map<scsi_defs::scsi_command, command_t*> commands;

	void AddCommand(scsi_defs::scsi_command, const char*, void (HostServices::*)(SASIDEV *));

	int AddRealtimeClockPage(int, BYTE *);
};
