//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsihd.h"
#include "scsihd_nec.h"
#include "scsimo.h"
#include "scsicd.h"
#include "scsi_printer.h"
#include "scsi_host_bridge.h"
#include "scsi_daynaport.h"
#include "rascsi_exceptions.h"
#include "host_services.h"
#include "device_factory.h"
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

using namespace std;
using namespace rascsi_interface;

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
	default_params[SCLP]["timeout"] = "30";

	extension_mapping["hd1"] = SCHD;
	extension_mapping["hds"] = SCHD;
	extension_mapping["hda"] = SCHD;
	extension_mapping["hdn"] = SCHD;
	extension_mapping["hdi"] = SCHD;
	extension_mapping["nhd"] = SCHD;
	extension_mapping["hdr"] = SCRM;
	extension_mapping["mos"] = SCMO;
	extension_mapping["iso"] = SCCD;

	device_mapping["bridge"] = SCBR;
	device_mapping["daynaport"] = SCDP;
	device_mapping["printer"] = SCLP;
	device_mapping["services"] = SCHS;
}

string DeviceFactory::GetExtension(const string& filename) const
{
	string ext;
	if (size_t separator = filename.rfind('.'); separator != string::npos) {
		ext = filename.substr(separator + 1);
	}
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

	return ext;
}

PbDeviceType DeviceFactory::GetTypeForFile(const string& filename) const
{
	if (const auto& it = extension_mapping.find(GetExtension(filename)); it != extension_mapping.end()) {
		return it->second;
	}

	if (const auto& it = device_mapping.find(filename); it != device_mapping.end()) {
		return it->second;
	}

	return UNDEFINED;
}

// ID -1 is used by rascsi to create a temporary device
shared_ptr<PrimaryDevice> DeviceFactory::CreateDevice(const ControllerManager& controller_manager, PbDeviceType type,
		int lun, const string& filename)
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
		if (string ext = GetExtension(filename); ext == "hdn" || ext == "hdi" || ext == "nhd") {
			device = make_shared<SCSIHD_NEC>(lun);
		} else {
			device = make_shared<SCSIHD>(lun, sector_sizes[SCHD], false,
					ext == "hd1" ? scsi_level::SCSI_1_CCS : scsi_level::SCSI_2);

			// Some Apple tools require a particular drive identification
			if (ext == "hda") {
				device->SetVendor("QUANTUM");
				device->SetProduct("FIREBALL");
			}
		}
		device->SetProtectable(true);
		device->SetStoppable(true);
		break;
	}

	case SCRM:
		device = make_shared<SCSIHD>(lun, sector_sizes[SCRM], true);
		device->SetProtectable(true);
		device->SetStoppable(true);
		device->SetRemovable(true);
		device->SetLockable(true);
		device->SetProduct("SCSI HD (REM.)");
		break;

	case SCMO:
		device = make_shared<SCSIMO>(lun, sector_sizes[SCMO]);
		device->SetProtectable(true);
		device->SetStoppable(true);
		device->SetRemovable(true);
		device->SetLockable(true);
		device->SetProduct("SCSI MO");
		break;

	case SCCD:
		device = make_shared<SCSICD>(lun, sector_sizes[SCCD]);
		device->SetReadOnly(true);
		device->SetStoppable(true);
		device->SetRemovable(true);
		device->SetLockable(true);
		device->SetProduct("SCSI CD-ROM");
		break;

	case SCBR:
		device = make_shared<SCSIBR>(lun);
		// Since this is an emulation for a specific driver the product name has to be set accordingly
		device->SetProduct("RASCSI BRIDGE");
		device->SupportsParams(true);
		device->SetDefaultParams(default_params[SCBR]);
		break;

	case SCDP:
		device = make_shared<SCSIDaynaPort>(lun);
		// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
		device->SetVendor("Dayna");
		device->SetProduct("SCSI/Link");
		device->SetRevision("1.4a");
		device->SupportsParams(true);
		device->SetDefaultParams(default_params[SCDP]);
		break;

	case SCHS:
		device = make_shared<HostServices>(lun, controller_manager);
		// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
		device->SetVendor("RaSCSI");
		device->SetProduct("Host Services");
		break;

	case SCLP:
		device = make_shared<SCSIPrinter>(lun);
		device->SetProduct("SCSI PRINTER");
		device->SupportsParams(true);
		device->SetDefaultParams(default_params[SCLP]);
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

const unordered_set<uint32_t>& DeviceFactory::GetSectorSizes(const string& type) const
{
	PbDeviceType t = UNDEFINED;
	PbDeviceType_Parse(type, &t);

	return GetSectorSizes(t);
}

const unordered_map<string, string>& DeviceFactory::GetDefaultParams(PbDeviceType type) const
{
	const auto& it = default_params.find(type);
	return it != default_params.end() ? it->second : empty_map;
}

list<string> DeviceFactory::GetNetworkInterfaces() const
{
	list<string> network_interfaces;

#ifdef __linux__
	ifaddrs *addrs;
	getifaddrs(&addrs);
	ifaddrs *tmp = addrs;

	while (tmp) {
	    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET &&
	    		strcmp(tmp->ifa_name, "lo") && strcmp(tmp->ifa_name, "rascsi_bridge")) {
	        int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	        ifreq ifr = {};
	        strcpy(ifr.ifr_name, tmp->ifa_name);
	        // Only list interfaces that are up
	        if (!ioctl(fd, SIOCGIFFLAGS, &ifr) && (ifr.ifr_flags & IFF_UP)) {
	        	network_interfaces.emplace_back(tmp->ifa_name);
	        }

	        close(fd);
	    }

	    tmp = tmp->ifa_next;
	}

	freeifaddrs(addrs);
#endif

	return network_interfaces;
}
