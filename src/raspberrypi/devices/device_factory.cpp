//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sasihd.h"
#include "scsihd.h"
#include "scsihd_nec.h"
#include "scsimo.h"
#include "scsicd.h"
#include "scsi_host_bridge.h"
#include "scsi_daynaport.h"
#include "device_factory.h"
#include <vector>

using namespace std;
using namespace rascsi_interface;

Device *DeviceFactory::CreateDevice(PbDeviceType& type, const string& filename, const string& ext)
{
	// If no type was specified try to derive the device type from the filename and extension
	if (type == UNDEFINED) {
		if (ext == "hdf") {
			type = SAHD;
		}
		else if (ext == "hds" || ext == "hdn" || ext == "hdi" || ext == "nhd" || ext == "hda") {
			type = SCHD;
		}
		else if (ext == "hdr") {
			type = SCRM;
		} else if (ext == "mos") {
			type = SCMO;
		} else if (ext == "iso") {
			type = SCCD;
		}
		else if (filename == "bridge") {
			type = SCBR;
		}
		else if (filename == "daynaport") {
			type = SCDP;
		}
	}

	vector<int> sector_sizes;
	sector_sizes.push_back(512);
	sector_sizes.push_back(1024);
	sector_sizes.push_back(2048);
	sector_sizes.push_back(4096);

	Device *device = NULL;
	switch (type) {
		case SAHD:
			device = new SASIHD();
			break;

		case SCHD:
			if (ext == "hdn" || ext == "hdi" || ext == "nhd") {
				device = new SCSIHD_NEC();
				((Disk *)device)->SetVendor("NEC");
			} else {
				device = new SCSIHD();
				device->SetProtectable(true);
				((Disk *)device)->SetSectorSizes(sector_sizes);
			}

			break;

		case SCRM:
			device = new SCSIHD(true);
			device->SetProtectable(true);
			device->SetLockable(true);
			device->SetProtectable(true);
			((Disk *)device)->SetSectorSizes(sector_sizes);
			break;

		case SCMO:
			device = new SCSIMO();
			device->SetProtectable(true);
			device->SetRemovable(true);
			device->SetLockable(true);
			device->SetProduct("SCSI MO");
			break;

		case SCCD:
			device = new SCSICD();
			device->SetReadOnly(true);
			device->SetRemovable(true);
			device->SetLockable(true);
			device->SetProduct("SCSI CD-ROM");
			break;

		case SCBR:
			device = new SCSIBR();
			device->SetProduct("BRIDGE");
			break;

		case SCDP:
			device = new SCSIDaynaPort();
			device->SetVendor("Dayna");
			device->SetProduct("SCSI/Link");
			device->SetRevision("1.4a");
			break;

		default:
			assert(false);
			break;
	}

	return device;
}
