//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
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

#include "controllers/scsidev_ctrl.h"
#include "disk.h"
#include "host_services.h"

using namespace scsi_defs;

HostServices::HostServices() : ModePageDevice("SCHS")
{
	dispatcher.AddCommand(eCmdTestUnitReady, "TestUnitReady", &HostServices::TestUnitReady);
	dispatcher.AddCommand(eCmdStartStop, "StartStopUnit", &HostServices::StartStopUnit);
}

bool HostServices::Dispatch(SCSIDEV *controller)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, controller) ? true : super::Dispatch(controller);
}

void HostServices::TestUnitReady(SCSIDEV *controller)
{
	// Always successful
	controller->Status();
}

int HostServices::Inquiry(const DWORD *cdb, BYTE *buf)
{
	// Processor device, SPC-5, not removable
	return PrimaryDevice::Inquiry(3, 7, false, cdb, buf);
}

void HostServices::StartStopUnit(SCSIDEV *controller)
{
	bool start = ctrl->cmd[4] & 0x01;
	bool load = ctrl->cmd[4] & 0x02;

	if (!start) {
		// Flush any caches
		for (Device *device : devices) {
			Disk *disk = dynamic_cast<Disk *>(device);
			if (disk) {
				disk->FlushCache();
			}
		}

		if (load) {
			controller->ScheduleShutDown(SCSIDEV::rascsi_shutdown_mode::STOP_PI);
		}
		else {
			controller->ScheduleShutDown(SCSIDEV::rascsi_shutdown_mode::STOP_RASCSI);
		}

		controller->Status();
		return;
	}
	else {
		if (load) {
			controller->ScheduleShutDown(SCSIDEV::rascsi_shutdown_mode::RESTART_PI);

			controller->Status();
			return;
		}
	}

	controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
}

int HostServices::ModeSense6(const DWORD *cdb, BYTE *buf)
{
	// Get length, clear buffer
	int length = (int)cdb[4];
	memset(buf, 0, length);

	// Get page code (0x00 is valid from the beginning)
	int page = cdb[2] & 0x3f;
	bool valid = page == 0x00;

	LOGTRACE("%s Requesting mode page $%02X", __PRETTY_FUNCTION__, page);

	// Basic information
	int size = 4;

	int ret = AddRealtimeClockPage(page, &buf[size]);
	if (ret > 0) {
		size += ret;
		valid = true;
	}

	if (!valid) {
		LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, page);
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}

	// Do not return more than ALLOCATION LENGTH bytes
	if (size > length) {
		LOGTRACE("%s %d bytes available, %d bytes requested", __PRETTY_FUNCTION__, size, length);
		size = length;
	}

	// Final setting of mode data length
	buf[0] = size;

	return size;
}

int HostServices::ModeSense10(const DWORD *cdb, BYTE *buf)
{
	// Get length, clear buffer
	int length = cdb[7];
	length <<= 8;
	length |= cdb[8];
	if (length > 0x800) {
		length = 0x800;
	}
	memset(buf, 0, length);

	// Get page code (0x00 is valid from the beginning)
	int page = cdb[2] & 0x3f;
	bool valid = page == 0x00;

	LOGTRACE("%s Requesting mode page $%02X", __PRETTY_FUNCTION__, page);

	// Basic Information
	int size = 8;

	int ret = AddRealtimeClockPage(page, &buf[size]);
	if (ret > 0) {
		size += ret;
		valid = true;
	}

	if (!valid) {
		LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, page);
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}

	// Do not return more than ALLOCATION LENGTH bytes
	if (size > length) {
		LOGTRACE("%s %d bytes available, %d bytes requested", __PRETTY_FUNCTION__, size, length);
		size = length;
	}

	// Final setting of mode data length
	buf[0] = size >> 8;
	buf[1] = size;

	return size;
}

int HostServices::AddRealtimeClockPage(int page, BYTE *buf)
{
	if (page == 0x20) {
		// Data structure version 1.0
		buf[0] = 0x01;
		buf[1] = 0x00;

		std::time_t t = std::time(NULL);
		std::tm tm = *std::localtime(&t);
		buf[2] = tm.tm_year;
		buf[3] = tm.tm_mon;
		buf[4] = tm.tm_mday;
		buf[5] = tm.tm_hour;
		buf[6] = tm.tm_min;
		// Ignore leap second for simplicity
		buf[7] = tm.tm_sec < 60 ? tm.tm_sec : 59;

		return 8;
	}

	return 0;
}
