//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_util.h"
#include "shared/network_util.h"
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
using namespace piscsi_interface;
using namespace piscsi_util;
using namespace network_util;

DeviceFactory::DeviceFactory()
{
	sector_sizes[SCHD] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCRM] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCMO] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCCD] = { 512, 2048};

	string network_interfaces;
	for (const auto& network_interface : GetNetworkInterfaces()) {
		if (!network_interfaces.empty()) {
			network_interfaces += ",";
		}
		network_interfaces += network_interface;
	}

	default_params[SCBR]["interface"] = network_interfaces;
	default_params[SCBR]["inet"] = DEFAULT_IP;
	default_params[SCDP]["interface"] = network_interfaces;
	default_params[SCDP]["inet"] = DEFAULT_IP;
	default_params[SCLP]["cmd"] = "lp -oraw %f";

	extension_mapping["hd1"] = SCHD;
	extension_mapping["hds"] = SCHD;
	extension_mapping["hda"] = SCHD;
	extension_mapping["hdn"] = SCHD;
	extension_mapping["hdi"] = SCHD;
	extension_mapping["nhd"] = SCHD;
	extension_mapping["hdr"] = SCRM;
	extension_mapping["mos"] = SCMO;
	extension_mapping["iso"] = SCCD;
	extension_mapping["is1"] = SCCD;

	device_mapping["bridge"] = SCBR;
	device_mapping["daynaport"] = SCDP;
	device_mapping["printer"] = SCLP;
	device_mapping["services"] = SCHS;
}

PbDeviceType DeviceFactory::GetTypeForFile(const string& filename) const
{
	if (const auto& it = extension_mapping.find(GetExtensionLowerCase(filename)); it != extension_mapping.end()) {
		return it->second;
	}

	if (const auto& it = device_mapping.find(filename); it != device_mapping.end()) {
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
			device = make_shared<SCSIHD>(lun, sector_sizes.find(type)->second, false,
					ext == "hd1" ? scsi_level::scsi_1_ccs : scsi_level::scsi_2);

			// Some Apple tools require a particular drive identification
			if (ext == "hda") {
				device->SetVendor("QUANTUM");
				device->SetProduct("FIREBALL");
			}
		}
		break;
	}

	case SCRM:
		device = make_shared<SCSIHD>(lun, sector_sizes.find(type)->second, true);
		device->SetProduct("SCSI HD (REM.)");
		break;

	case SCMO:
		device = make_shared<SCSIMO>(lun, sector_sizes.find(type)->second);
		device->SetProduct("SCSI MO");
		break;

	case SCCD:
		device = make_shared<SCSICD>(lun, sector_sizes.find(type)->second,
            GetExtensionLowerCase(filename) == "is1" ? scsi_level::scsi_1_ccs : scsi_level::scsi_2);
		device->SetProduct("SCSI CD-ROM");
		break;

	case SCBR:
		device = make_shared<SCSIBR>(lun);
		// Since this is an emulation for a specific driver the product name has to be set accordingly
		device->SetProduct("RASCSI BRIDGE");
		device->SetDefaultParams(default_params.find(type)->second);
		break;

	case SCDP:
		device = make_shared<SCSIDaynaPort>(lun);
		// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
		device->SetVendor("Dayna");
		device->SetProduct("SCSI/Link");
		device->SetRevision("1.4a");
		device->SetDefaultParams(default_params.find(type)->second);
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
		device->SetDefaultParams(default_params.find(type)->second);
		break;

	default:
		break;
	}

	return device;
}

const unordered_set<uint32_t>& DeviceFactory::GetSectorSizes(PbDeviceType type) const
{
	const auto& it = sector_sizes.find(type);
	return it != sector_sizes.end() ? it->second : empty_set;
}

const unordered_map<string, string>& DeviceFactory::GetDefaultParams(PbDeviceType type) const
{
	const auto& it = default_params.find(type);
	return it != default_params.end() ? it->second : empty_map;
}
