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

	switch (type) {
		case SAHD:
			return new SASIHD();

		case SCHD:
			if (ext == "hdn" || ext == "hdi" || ext == "nhd") {
				return new SCSIHD_NEC();
			} else {
				return new SCSIHD();
			}

		case SCRM:
			return new SCSIHD(true);

		case SCMO:
			return new SCSIMO();

		case SCCD:
			return new SCSICD();

		case SCBR:
			return new SCSIBR();

		case SCDP:
			return new SCSIDaynaPort();

		default:
			return NULL;
	}
}
