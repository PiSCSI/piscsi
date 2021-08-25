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
string ListDevices(const PbDevices& devices)
{
	ostringstream s;

	if (devices.devices_size()) {
		s << "+----+----+------+-------------------------------------" << endl
			<< "| ID | UN | TYPE | DEVICE STATUS" << endl
			<< "+----+----+------+-------------------------------------" << endl;
	}
	else {
		return "No images currently attached.";
	}

	list<PbDevice> sorted_devices;
	for (int i = 0; i < devices.devices_size(); i++) {
		sorted_devices.push_back(devices.devices(i));
	}
	sorted_devices.sort([](const PbDevice& a, const PbDevice& b) { return a.id() < b.id(); });

	for (auto it = sorted_devices.begin(); it != sorted_devices.end(); ++it) {
		const PbDevice& device = *it;

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
