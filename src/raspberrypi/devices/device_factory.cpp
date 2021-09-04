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
#include "exceptions.h"
#include "device_factory.h"
#include <set>
#include <map>

using namespace std;
using namespace rascsi_interface;

DeviceFactory::DeviceFactory()
{
	sector_sizes_sasi.insert(256);
	sector_sizes_sasi.insert(1024);

	sector_sizes_scsi.insert(512);
	sector_sizes_scsi.insert(1024);
	sector_sizes_scsi.insert(2048);
	sector_sizes_scsi.insert(4096);

	// 128 MB, 512 bytes per sector, 248826 sectors
	geometries_mo[0x797f400] = make_pair(512, 248826);
	// 230 MB, 512 bytes per block, 446325 sectors
	geometries_mo[0xd9eea00] = make_pair(512, 446325);
	// 540 MB, 512 bytes per sector, 1041500 sectors
	geometries_mo[0x1fc8b800] = make_pair(512, 1041500);
	// 640 MB, 20248 bytes per sector, 310352 sectors
	geometries_mo[0x25e28000] = make_pair(2048, 310352);
}

DeviceFactory& DeviceFactory::instance()
{
	static DeviceFactory instance;
	return instance;
}

Device *DeviceFactory::CreateDevice(PbDeviceType type, const string& filename, const string& ext)
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
		else {
			return NULL;
		}
	}

	Device *device = NULL;
	try {
		switch (type) {
			case SAHD:
				device = new SASIHD();
				((Disk *)device)->SetSectorSizes(sector_sizes_sasi);
			break;

			case SCHD:
				if (ext == "hdn" || ext == "hdi" || ext == "nhd") {
					device = new SCSIHD_NEC();
					((Disk *)device)->SetVendor("NEC");
				} else {
					device = new SCSIHD(false);
					device->SetProtectable(true);
					((Disk *)device)->SetSectorSizes(sector_sizes_scsi);
				}
				break;

			case SCRM:
				device = new SCSIHD(true);
				device->SetRemovable(true);
				device->SetLockable(true);
				device->SetProtectable(true);
				device->SetProduct("SCSI HD (REM.)");
				((Disk *)device)->SetSectorSizes(sector_sizes_scsi);
				break;

			case SCMO:
				device = new SCSIMO();
				device->SetProtectable(true);
				device->SetRemovable(true);
				device->SetLockable(true);
				device->SetProduct("SCSI MO");
				((Disk *)device)->SetGeometries(geometries_mo);
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
				device->SetProduct("SCSI HOST BRIDGE");
				break;

			case SCDP:
				device = new SCSIDaynaPort();
				// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
				device->SetVendor("Dayna");
				device->SetProduct("SCSI/Link");
				device->SetRevision("1.4a");
				break;

			default:
				break;
		}
	}
	catch(const illegal_argument_exception& e) {
		// There was an internal problem with setting up the device data
		return NULL;
	}

	return device;
}

const set<uint64_t> DeviceFactory::GetMoCapacities() const
{
	set<uint64_t> keys;

	for (const auto& geometry : geometries_mo) {
		keys.insert(geometry.first);
	}

	return keys;
}
