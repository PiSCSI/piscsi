//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <list>
#include <sstream>
#include "rascsi_interface.pb.h"
#include "rasutil.h"

using namespace std;
using namespace rascsi_interface;

//---------------------------------------------------------------------------
//
//	List devices
//
//---------------------------------------------------------------------------
string ListDevices(const PbServerInfo& server_info)
{
	if (!server_info.devices_size()) {
		return "No images currently attached.";
	}

	ostringstream s;
	s << "+----+----+------+-------------------------------------" << endl
			<< "| ID | UN | TYPE | DEVICE STATUS" << endl
			<< "+----+----+------+-------------------------------------" << endl;

	list<PbDevice> devices = { server_info.devices().begin(), server_info.devices().end() };
	devices.sort([](const auto& a, const auto& b) { return a.id() < b.id() && a.unit() < b.unit(); });

	for (const auto& device : devices) {
		string filename;
		switch (device.type()) {
			case SCBR:
				filename = "X68000 HOST BRIDGE";
				break;

			case SCDP:
				filename = "DaynaPort SCSI/Link";
				break;

			default:
				filename = device.file().name();
				break;
		}

		s << "|  " << device.id() << " |  " << device.unit() << " | " << PbDeviceType_Name(device.type()) << " | "
				<< (filename.empty() ? "NO MEDIA" : filename)
				<< (!device.status().removed() && (device.properties().read_only() || device.status().protected_()) ? " (WRITEPROTECT)" : "")
				<< endl;
	}

	s << "+----+----+------+-------------------------------------";

	return s.str();
}
