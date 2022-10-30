//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-22 Uwe Seimet
// Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "rascsi_version.h"
#include "list_devices.h"
#include <sstream>

using namespace std;
using namespace rascsi_interface;

string ras_util::ListDevices(const list<PbDevice>& pb_devices)
{
	if (pb_devices.empty()) {
		return "No devices currently attached.\n";
	}

	ostringstream s;
	s << "+----+-----+------+-------------------------------------\n"
			<< "| ID | LUN | TYPE | IMAGE FILE\n"
			<< "+----+-----+------+-------------------------------------\n";

	list<PbDevice> devices = pb_devices;
	devices.sort([](const auto& a, const auto& b) { return a.id() < b.id() || a.unit() < b.unit(); });

	for (const auto& device : devices) {
		string filename;
		switch (device.type()) {
			case SCBR:
				filename = "X68000 HOST BRIDGE";
				break;

			case SCDP:
				filename = "DaynaPort SCSI/Link";
				break;

			case SCHS:
				filename = "Host Services";
				break;

			case SCLP:
				filename = "SCSI Printer";
				break;

			default:
				filename = device.file().name();
				break;
		}

		s << "|  " << device.id() << " |   " << device.unit() << " | " << PbDeviceType_Name(device.type()) << " | "
				<< (filename.empty() ? "NO MEDIUM" : filename)
				<< (!device.status().removed() && (device.properties().read_only() || device.status().protected_()) ? " (READ-ONLY)" : "")
				<< '\n';
	}

	s << "+----+-----+------+-------------------------------------\n";

	return s.str();
}
