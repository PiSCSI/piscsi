//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
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
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>

using namespace std;
using namespace piscsi_interface;
using namespace piscsi_util;

DeviceFactory::DeviceFactory()
{
	sector_sizes[SCHD] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCRM] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCMO] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCCD] = { 512, 2048};

	string network_interfaces;
	for (const auto& network_interface : GetNetworkInterfaces()) {
		if (network_interface.rfind("dummy", 0) == string::npos) {
			if (!network_interfaces.empty()) {
				network_interfaces += ",";
			}
			network_interfaces += network_interface;
		}
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
			device = make_shared<SCSIHD>(lun, sector_sizes.find(SCHD)->second, false,
					ext == "hd1" ? scsi_level::SCSI_1_CCS : scsi_level::SCSI_2);

			// Some Apple tools require a particular drive identification
			if (ext == "hda") {
				device->SetVendor("QUANTUM");
				device->SetProduct("FIREBALL");
			}
		}
		break;
	}

	case SCRM:
		device = make_shared<SCSIHD>(lun, sector_sizes.find(SCRM)->second, true);
		device->SetProduct("SCSI HD (REM.)");
		break;

	case SCMO:
		device = make_shared<SCSIMO>(lun, sector_sizes.find(SCMO)->second);
		device->SetProduct("SCSI MO");
		break;

	case SCCD:
		device = make_shared<SCSICD>(lun, sector_sizes.find(SCCD)->second,
            GetExtensionLowerCase(filename) == "is1" ? scsi_level::SCSI_1_CCS : scsi_level::SCSI_2);
		device->SetProduct("SCSI CD-ROM");
		break;

	case SCBR:
		device = make_shared<SCSIBR>(lun);
		// Since this is an emulation for a specific driver the product name has to be set accordingly
		device->SetProduct("RASCSI BRIDGE");
		device->SetDefaultParams(default_params.find(SCBR)->second);
		break;

	case SCDP:
		device = make_shared<SCSIDaynaPort>(lun);
		// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
		device->SetVendor("Dayna");
		device->SetProduct("SCSI/Link");
		device->SetRevision("1.4a");
		device->SetDefaultParams(default_params.find(SCDP)->second);
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
		device->SetDefaultParams(default_params.find(SCLP)->second);
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

vector<string> DeviceFactory::GetNetworkInterfaces() const
{
	vector<string> network_interfaces;

#ifdef __linux__
	ifaddrs *addrs;
	getifaddrs(&addrs);
	ifaddrs *tmp = addrs;

	while (tmp) {
	    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET &&
	    		strcmp(tmp->ifa_name, "lo") && strcmp(tmp->ifa_name, "piscsi_bridge")) {
	        const int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	        ifreq ifr = {};
	        strcpy(ifr.ifr_name, tmp->ifa_name); //NOSONAR Using strcpy is safe here
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
