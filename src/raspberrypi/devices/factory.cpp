//---------------------------------------------------------------------------
//
//      SCSI Target Emulator RaSCSI (*^..^*)
//      for Raspberry Pi
//
//      Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "sasihd.h"
#include "scsihd.h"
#include "scsihd_nec.h"
#include "scsihd_apple.h"
#include "scsimo.h"
#include "scsicd.h"
#include "scsi_host_bridge.h"
#include "scsi_daynaport.h"
#include "factory.h"

using namespace rascsi_interface;

Device *DeviceFactory::CreateDevice(PbDeviceType& type, const std::string& ext)
{
	// If no type was specified try to derive the device type from the file extension
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
	}

	switch (type) {
		case SAHD:
			return new SASIHD();

		case SCHD:
			if (ext == "hdn" || ext == "hdi" || ext == "nhd") {
				return new SCSIHD_NEC();
			} else if (ext == "hda") {
				return new SCSIHD_APPLE();
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
