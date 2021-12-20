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

bool ras_util::GetAsInt(const string& value, int& result)
{
	if (value.find_first_not_of("0123456789") != string::npos) {
		return false;
	}

	try {
		result = std::stoul(value);
	}
	catch(const invalid_argument& e) {
		return false;
	}
	catch(const out_of_range& e) {
		return false;
	}

	return true;
}

string ras_util::ListDevices(const list<PbDevice>& pb_devices)
{
	if (pb_devices.empty()) {
		return "No images currently attached.";
	}

	ostringstream s;
	s << "+----+-----+------+-------------------------------------" << endl
			<< "| ID | LUN | TYPE | IMAGE FILE" << endl
			<< "+----+-----+------+-------------------------------------" << endl;

	list<PbDevice> devices = pb_devices;
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

		s << "|  " << device.id() << " |   " << device.unit() << " | " << PbDeviceType_Name(device.type()) << " | "
				<< (filename.empty() ? "NO MEDIA" : filename)
				<< (!device.status().removed() && (device.properties().read_only() || device.status().protected_()) ? " (READ-ONLY)" : "")
				<< endl;
	}

	s << "+----+-----+------+-------------------------------------";

	return s.str();
}
