//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

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
		s << "+----+----+------+---------------+---------------------" << endl
			<< "| ID | UN | TYPE | DEVICE STATUS | IMAGE" << endl
			<< "+----+----+------+---------------+---------------------" << endl;
	}
	else {
		return "No images currently attached.";
	}

	for (int i = 0; i < devices.devices_size() ; i++) {
		PbDevice device = devices.devices(i);

		string filename;
		string status;
		
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
		
		if ((!device.removed() && (device.read_only() || device.protected_())) {
			status = "WRITEPROTECT";
		}
		else if (filename.empty()) {
			status = "NO MEDIA";
		}
		else if (!device.removed()) {
			status = "ATTACHED";
		}
		else {
			status = "";
		}
		
		s << "|  " << device.id() << " |  " << device.unit() << " | "
				<< PbDeviceType_Name(device.type()) << " | "
				<< status << " | " << filename << endl;s
	}

	s << "+----+----+------+-------------------------------------";

	return s.str();
}
