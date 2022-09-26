//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Host Services with realtime clock and shutdown support
//
//---------------------------------------------------------------------------

//
// Features of the host services device:
//
// 1. Vendor-specific mode page 0x20 returns the current date and time, see mode_page_datetime
//
// 2. START/STOP UNIT shuts down RaSCSI or shuts down/reboots the Raspberry Pi
//   a) !start && !load (STOP): Shut down RaSCSI
//   b) !start && load (EJECT): Shut down the Raspberry Pi
//   c) start && load (LOAD): Reboot the Raspberry Pi
//

#include "rascsi_exceptions.h"
#include "device_factory.h"
#include "scsi_command_util.h"
#include "dispatcher.h"
#include "host_services.h"
#include <algorithm>

using namespace scsi_defs;
using namespace scsi_command_util;

HostServices::HostServices(const DeviceFactory& factory) : ModePageDevice("SCHS"), device_factory(factory)
{
	dispatcher.Add(scsi_command::eCmdTestUnitReady, "TestUnitReady", &HostServices::TestUnitReady);
	dispatcher.Add(scsi_command::eCmdStartStop, "StartStopUnit", &HostServices::StartStopUnit);
}

bool HostServices::Dispatch(scsi_command cmd)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, cmd) ? true : super::Dispatch(cmd);
}

void HostServices::TestUnitReady()
{
	// Always successful
	EnterStatusPhase();
}

vector<byte> HostServices::InquiryInternal() const
{
	return HandleInquiry(device_type::PROCESSOR, scsi_level::SPC_3, false);
}

void HostServices::StartStopUnit()
{
	bool start = ctrl->cmd[4] & 0x01;
	bool load = ctrl->cmd[4] & 0x02;

	if (!start) {
		// Flush any caches
		for (PrimaryDevice *device : device_factory.GetAllDevices()) {
			device->FlushCache();
		}

		if (load) {
			controller->ScheduleShutdown(ScsiController::rascsi_shutdown_mode::STOP_PI);
		}
		else {
			controller->ScheduleShutdown(ScsiController::rascsi_shutdown_mode::STOP_RASCSI);
		}

		EnterStatusPhase();
		return;
	}
	else {
		if (load) {
			controller->ScheduleShutdown(ScsiController::rascsi_shutdown_mode::RESTART_PI);

			EnterStatusPhase();
			return;
		}
	}

	throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
}

int HostServices::ModeSense6(const vector<int>& cdb, vector<BYTE>& buf) const
{
	// Block descriptors cannot be returned
	if (!(cdb[1] & 0x08)) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	auto length = (int)min(buf.size(), (size_t)cdb[4]);
	fill_n(buf.begin(), length, 0);

	// Basic Information
	int size = 4;

	size += super::AddModePages(cdb, buf, size, length - size);
	if (size > 255) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Do not return more than ALLOCATION LENGTH bytes
	if (size > length) {
		size = length;
	}

	buf[0] = (BYTE)size;

	return size;
}

int HostServices::ModeSense10(const vector<int>& cdb, vector<BYTE>& buf) const
{
	// Block descriptors cannot be returned
	if (!(cdb[1] & 0x08)) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	auto length = (int)min(buf.size(), (size_t)GetInt16(cdb, 7));
	fill_n(buf.begin(), length, 0);

	// Basic Information
	int size = 8;

	size += super::AddModePages(cdb, buf, size, length - size);
	if (size > 65535) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Do not return more than ALLOCATION LENGTH bytes
	if (size > length) {
		size = length;
	}

	SetInt16(buf, 0, size);

	return size;
}

void HostServices::SetUpModePages(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	if (page == 0x20 || page == 0x3f) {
		AddRealtimeClockPage(pages, changeable);
	}
}

void HostServices::AddRealtimeClockPage(map<int, vector<byte>>& pages, bool changeable) const
{
	if (!changeable) {
		time_t t = time(nullptr);
		tm localtime;
		localtime_r(&t, &localtime);

		mode_page_datetime datetime;
		datetime.major_version = 0x01;
		datetime.minor_version = 0x00;
		datetime.year = (uint8_t)localtime.tm_year;
		datetime.month = (uint8_t)localtime.tm_mon;
		datetime.day = (uint8_t)localtime.tm_mday;
		datetime.hour = (uint8_t)localtime.tm_hour;
		datetime.minute = (uint8_t)localtime.tm_min;
		// Ignore leap second for simplicity
		datetime.second = (uint8_t)(localtime.tm_sec < 60 ? localtime.tm_sec : 59);

		vector<byte> buf(10);
		memcpy(&buf[2], &datetime, sizeof(datetime));
		pages[32] = buf;
	}
}
