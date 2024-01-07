//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_util.h"
#include "scsihd.h"
#include "scsihd_nec.h"
#include "scsimo.h"
#include "scsicd.h"
#include "scsi_printer.h"
#include "scsi_host_bridge.h"
#include "scsi_daynaport.h"
#include "host_services.h"
#include "device_factory.h"

using namespace std;
using namespace piscsi_util;

PbDeviceType DeviceFactory::GetTypeForFile(const string& filename) const
{
	if (const auto& it = EXTENSION_MAPPING.find(GetExtensionLowerCase(filename)); it != EXTENSION_MAPPING.end()) {
		return it->second;
	}

	if (const auto& it = DEVICE_MAPPING.find(filename); it != DEVICE_MAPPING.end()) {
		return it->second;
	}

	return UNDEFINED;
}

shared_ptr<PrimaryDevice> DeviceFactory::CreateDevice(PbDeviceType type, int lun, const string& filename) const
{
	// If no type was specified try to derive the device type from the filename
	if (type == UNDEFINED) {
		type = GetTypeForFile(filename);
		if (type == UNDEFINED) {
			return nullptr;
		}
	}

	shared_ptr<PrimaryDevice> device;
	switch (type) {
	case SCHD: {
		if (const string ext = GetExtensionLowerCase(filename); ext == "hdn" || ext == "hdi" || ext == "nhd") {
			device = make_shared<SCSIHD_NEC>(lun);
		} else {
			device = make_shared<SCSIHD>(lun, false, ext == "hd1" ? scsi_level::scsi_1_ccs : scsi_level::scsi_2);

			// Some Apple tools require a particular drive identification
			if (ext == "hda") {
				device->SetVendor("QUANTUM");
				device->SetProduct("FIREBALL");
			}
		}
		break;
	}

	case SCRM:
		device = make_shared<SCSIHD>(lun, true, scsi_level::scsi_2);
		device->SetProduct("SCSI HD (REM.)");
		break;

	case SCMO:
		device = make_shared<SCSIMO>(lun);
		device->SetProduct("SCSI MO");
		break;

	case SCCD:
		device = make_shared<SCSICD>(lun,
            GetExtensionLowerCase(filename) == "is1" ? scsi_level::scsi_1_ccs : scsi_level::scsi_2);
		device->SetProduct("SCSI CD-ROM");
		break;

	case SCBR:
		device = make_shared<SCSIBR>(lun);
		// Since this is an emulation for a specific driver the product name has to be set accordingly
		device->SetProduct("RASCSI BRIDGE");
		break;

	case SCDP:
		device = make_shared<SCSIDaynaPort>(lun);
		// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
		device->SetVendor("Dayna");
		device->SetProduct("SCSI/Link");
		device->SetRevision("1.4a");
		break;

	case SCHS:
		device = make_shared<HostServices>(lun);
		// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
		device->SetVendor("PiSCSI");
		device->SetProduct("Host Services");
		break;

	case SCLP:
		device = make_shared<SCSIPrinter>(lun);
		device->SetProduct("SCSI PRINTER");
		break;

	default:
		break;
	}

	return device;
}
