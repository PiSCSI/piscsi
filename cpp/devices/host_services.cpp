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

#include "controllers/controller_manager.h"
#include "controllers/scsi_controller.h"
#include "rascsi_exceptions.h"
#include "scsi_command_util.h"
#include "dispatcher.h"
#include "host_services.h"
#include <algorithm>

using namespace scsi_defs;
using namespace scsi_command_util;

HostServices::HostServices(int lun, const ControllerManager& manager)
	: ModePageDevice(SCHS, lun), controller_manager(manager)
{
	dispatcher.Add(scsi_command::eCmdTestUnitReady, "TestUnitReady", &HostServices::TestUnitReady);
	dispatcher.Add(scsi_command::eCmdStartStop, "StartStopUnit", &HostServices::StartStopUnit);

	SetReady(true);
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
	const bool start = controller->GetCmd(4) & 0x01;
	const bool load = controller->GetCmd(4) & 0x02;

	if (!start) {
		// Flush any caches
		for (const auto& device : controller_manager.GetAllDevices()) {
			device->FlushCache();
		}

		if (load) {
			controller->ScheduleShutdown(AbstractController::rascsi_shutdown_mode::STOP_PI);
		}
		else {
			controller->ScheduleShutdown(AbstractController::rascsi_shutdown_mode::STOP_RASCSI);
		}
	}
	else if (load) {
		controller->ScheduleShutdown(AbstractController::rascsi_shutdown_mode::RESTART_PI);
	}
	else {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	EnterStatusPhase();
}

int HostServices::ModeSense6(const vector<int>& cdb, vector<uint8_t>& buf) const
{
	// Block descriptors cannot be returned
	if (!(cdb[1] & 0x08)) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(cdb[4])));
	fill_n(buf.begin(), length, 0);

	// 4 bytes basic information
	int size = AddModePages(cdb, buf, 4, length, 255);

	buf[0] = (uint8_t)size;

	return size;
}

int HostServices::ModeSense10(const vector<int>& cdb, vector<uint8_t>& buf) const
{
	// Block descriptors cannot be returned
	if (!(cdb[1] & 0x08)) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(GetInt16(cdb, 7))));
	fill_n(buf.begin(), length, 0);

	// 8 bytes basic information
	int size = AddModePages(cdb, buf, 8, length, 65535);

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
	vector<byte> buf(10);

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

		memcpy(&buf[2], &datetime, sizeof(datetime));
	}

	pages[32] = buf;
}
