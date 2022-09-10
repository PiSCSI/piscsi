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
// 1. Vendor-specific mode page 0x20 returns the current date and time (realtime clock)
//
//  typedef struct {
//    // Major and minor version of this data structure
//    uint8_t major_version;
//    uint8_t minor_version;
//    // Current date and time, with daylight savings time adjustment applied
//    uint8_t year; // year - 1900
//    uint8_t month; // 0-11
//    uint8_t day; // 1-31
//    uint8_t hour; // 0-23
//    uint8_t minute; // 0-59
//    uint8_t second; // 0-59
//  } mode_page_datetime;
//
// 2. START/STOP UNIT shuts down RaSCSI or shuts down/reboots the Raspberry Pi
//   a) !start && !load (STOP): Shut down RaSCSI
//   b) !start && load (EJECT): Shut down the Raspberry Pi
//   c) start && load (LOAD): Reboot the Raspberry Pi
//

#include "rascsi_exceptions.h"
#include "device.h"
#include "host_services.h"

using namespace scsi_defs;

HostServices::HostServices() : ModePageDevice("SCHS")
{
	dispatcher.AddCommand(eCmdTestUnitReady, "TestUnitReady", &HostServices::TestUnitReady);
	dispatcher.AddCommand(eCmdStartStop, "StartStopUnit", &HostServices::StartStopUnit);
}

bool HostServices::Dispatch()
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, ctrl->cmd[0]) ? true : super::Dispatch();
}

void HostServices::TestUnitReady()
{
	// Always successful
	EnterStatusPhase();
}

vector<BYTE> HostServices::InquiryInternal() const
{
	return HandleInquiry(device_type::PROCESSOR, scsi_level::SPC_3, false);
}

void HostServices::StartStopUnit()
{
	bool start = ctrl->cmd[4] & 0x01;
	bool load = ctrl->cmd[4] & 0x02;

	if (!start) {
		// Flush any caches
		for (Device *device : devices) {
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

int HostServices::ModeSense6(const DWORD *cdb, BYTE *buf, int max_length)
{
	// Block descriptors cannot be returned
	if (!(cdb[1] & 0x08)) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	auto length = (int)cdb[4];
	if (length > max_length) {
		length = max_length;
	}
	memset(buf, 0, length);

	// Basic Information
	int size = 4;

	size += super::AddModePages(cdb, &buf[size], length - size);
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

int HostServices::ModeSense10(const DWORD *cdb, BYTE *buf, int max_length)
{
	// Block descriptors cannot be returned
	if (!(cdb[1] & 0x08)) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	int length = (cdb[7] << 8) | cdb[8];
	if (length > max_length) {
		length = max_length;
	}
	memset(buf, 0, length);

	// Basic Information
	int size = 8;

	size += super::AddModePages(cdb, &buf[size], length - size);
	if (size > 65535) {
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Do not return more than ALLOCATION LENGTH bytes
	if (size > length) {
		size = length;
	}

	buf[0] = (BYTE)(size >> 8);
	buf[1] = (BYTE)size;

	return size;
}

void HostServices::AddModePages(map<int, vector<BYTE>>& pages, int page, bool changeable) const
{
	if (page == 0x20 || page == 0x3f) {
		AddRealtimeClockPage(pages, changeable);
	}
}

void HostServices::AddRealtimeClockPage(map<int, vector<BYTE>>& pages, bool changeable) const
{
	if (!changeable) {
		vector<BYTE> buf(10);

		// Data structure version 1.0
		buf[2] = 0x01;
		buf[3] = 0x00;

		std::time_t t = std::time(nullptr);
		std::tm tm = *std::localtime(&t);
		buf[4] = (BYTE)tm.tm_year;
		buf[5] = (BYTE)tm.tm_mon;
		buf[6] = (BYTE)tm.tm_mday;
		buf[7] = (BYTE)tm.tm_hour;
		buf[8] = (BYTE)tm.tm_min;
		// Ignore leap second for simplicity
		buf[9] = (BYTE)(tm.tm_sec < 60 ? tm.tm_sec : 59);

		pages[32] = buf;
	}
}
