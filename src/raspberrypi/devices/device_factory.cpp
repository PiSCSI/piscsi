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
#include <ifaddrs.h>
#include <set>
#include <map>

using namespace std;
using namespace rascsi_interface;

DeviceFactory::DeviceFactory()
{
	sector_sizes[SAHD] = { 256, 1024 };
	sector_sizes[SCHD] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCRM] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCMO] = { 512, 1024, 2048, 4096 };
	// Some old Sun CD-ROM drives support 512 bytes per sector
	sector_sizes[SCCD] = { 512, 2048};
	sector_sizes[SCBR] = {};
	sector_sizes[SCDP] = {};

	// 128 MB, 512 bytes per sector, 248826 sectors
	geometries[SCMO][0x797f400] = make_pair(512, 248826);
	// 230 MB, 512 bytes per block, 446325 sectors
	geometries[SCMO][0xd9eea00] = make_pair(512, 446325);
	// 540 MB, 512 bytes per sector, 1041500 sectors
	geometries[SCMO][0x1fc8b800] = make_pair(512, 1041500);
	// 640 MB, 20248 bytes per sector, 310352 sectors
	geometries[SCMO][0x25e28000] = make_pair(2048, 310352);
	geometries[SAHD] = {};
	geometries[SCHD] = {};
	geometries[SCRM] = {};
	geometries[SCCD] = {};
	geometries[SCBR] = {};
	geometries[SCDP] = {};

	string network_interfaces;
	for (const auto& network_interface : GetNetworkInterfaces()) {
		if (!network_interfaces.empty()) {
			network_interfaces += ",";
		}
		network_interfaces += network_interface;
	}

	default_params[SAHD] = {};
	default_params[SCHD] = {};
	default_params[SCRM] = {};
	default_params[SCMO] = {};
	default_params[SCCD] = {};
	default_params[SCBR]["interfaces"] = network_interfaces;
	default_params[SCDP]["interfaces"] = network_interfaces;

	extension_mapping["hdf"] = SAHD;
	extension_mapping["hds"] = SCHD;
	extension_mapping["hda"] = SCHD;
	extension_mapping["hdn"] = SCHD;
	extension_mapping["hdi"] = SCHD;
	extension_mapping["nhd"] = SCHD;
	extension_mapping["hdr"] = SCRM;
	extension_mapping["mos"] = SCMO;
	extension_mapping["iso"] = SCCD;
}

DeviceFactory& DeviceFactory::instance()
{
	static DeviceFactory instance;
	return instance;
}

string DeviceFactory::GetExtension(const string& filename) const
{
	string ext;
	size_t separator = filename.rfind('.');
	if (separator != string::npos) {
		ext = filename.substr(separator + 1);
	}
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

	return ext;
}

PbDeviceType DeviceFactory::GetTypeForFile(const string& filename)
{
	string ext = GetExtension(filename);

	if (extension_mapping.find(ext) != extension_mapping.end()) {
		return extension_mapping[ext];
	}
	else if (filename == "bridge") {
		return SCBR;
	}
	else if (filename == "daynaport") {
		return SCDP;
	}

	return UNDEFINED;
}

Device *DeviceFactory::CreateDevice(PbDeviceType type, const string& filename)
{
	// If no type was specified try to derive the device type from the filename
	if (type == UNDEFINED) {
		type = GetTypeForFile(filename);
		if (type == UNDEFINED) {
			return NULL;
		}
	}

	Device *device = NULL;
	try {
		switch (type) {
			case SAHD:
				device = new SASIHD();
				device->SetSupportedLuns(2);
				device->SetProduct("SASI HD");
				((Disk *)device)->SetSectorSizes(sector_sizes[SAHD]);
			break;

			case SCHD: {
				string ext = GetExtension(filename);
				if (ext == "hdn" || ext == "hdi" || ext == "nhd") {
					device = new SCSIHD_NEC();
					((Disk *)device)->SetSectorSizes({ 512 });
				} else {
					device = new SCSIHD(false);
					((Disk *)device)->SetSectorSizes(sector_sizes[SCHD]);

					// Some Apple tools require a particular drive identification
					if (ext == "hda") {
						device->SetVendor("QUANTUM");
						device->SetProduct("FIREBALL");
					}
				}
				device->SetSupportedLuns(32);
				device->SetProtectable(true);
				device->SetStoppable(true);
				break;
			}

			case SCRM:
				device = new SCSIHD(true);
				device->SetSupportedLuns(32);
				device->SetProtectable(true);
				device->SetStoppable(true);
				device->SetRemovable(true);
				device->SetLockable(true);
				device->SetProduct("SCSI HD (REM.)");
				((Disk *)device)->SetSectorSizes(sector_sizes[SCRM]);
				break;

			case SCMO:
				device = new SCSIMO();
				device->SetSupportedLuns(32);
				device->SetProtectable(true);
				device->SetStoppable(true);
				device->SetRemovable(true);
				device->SetLockable(true);
				device->SetProduct("SCSI MO");
				((Disk *)device)->SetSectorSizes(sector_sizes[SCRM]);
				((Disk *)device)->SetGeometries(geometries[SCMO]);
				break;

			case SCCD:
				device = new SCSICD();
				device->SetSupportedLuns(32);
				device->SetReadOnly(true);
				device->SetStoppable(true);
				device->SetRemovable(true);
				device->SetLockable(true);
				device->SetProduct("SCSI CD-ROM");
				((Disk *)device)->SetSectorSizes(sector_sizes[SCCD]);
				break;

			case SCBR:
				device = new SCSIBR();
				device->SetSupportedLuns(1);
				device->SetProduct("SCSI HOST BRIDGE");
				device->SupportsParams(true);
				device->SetDefaultParams(default_params[SCBR]);
				break;

			case SCDP:
				device = new SCSIDaynaPort();
				device->SetSupportedLuns(1);
				// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
				device->SetVendor("Dayna");
				device->SetProduct("SCSI/Link");
				device->SetRevision("1.4a");
				device->SupportsParams(true);
				device->SetDefaultParams(default_params[SCDP]);
				break;

			default:
				break;
		}
	}
	catch(const illegal_argument_exception& e) {
		// There was an internal problem with setting up the device data for INQUIRY
		return NULL;
	}

	return device;
}

const set<uint32_t>& DeviceFactory::GetSectorSizes(const string& type)
{
	PbDeviceType t = UNDEFINED;
	PbDeviceType_Parse(type, &t);
	return sector_sizes[t];
}

const set<uint64_t> DeviceFactory::GetCapacities(PbDeviceType type)
{
	set<uint64_t> keys;

	for (const auto& geometry : geometries[type]) {
		keys.insert(geometry.first);
	}

	return keys;
}

const list<string> DeviceFactory::GetNetworkInterfaces() const
{
	list<string> network_interfaces;

	struct ifaddrs *addrs;
	getifaddrs(&addrs);
	struct ifaddrs *tmp = addrs;

	while (tmp) {
	    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET &&
	    		strcmp(tmp->ifa_name, "lo") && strcmp(tmp->ifa_name, "rascsi_bridge")) {
	        int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	        struct ifreq ifr;
	        memset(&ifr, 0, sizeof(ifr));

	        strcpy(ifr.ifr_name, tmp->ifa_name);
	        if (!ioctl(fd, SIOCGIFFLAGS, &ifr)) {
	        	close(fd);

	        	// Only list interfaces that are up
	        	if (ifr.ifr_flags & IFF_UP) {
	        		network_interfaces.push_back(tmp->ifa_name);
	        	}
	        }
	        else {
	        	close(fd);
	        }
	    }

	    tmp = tmp->ifa_next;
	}

	freeifaddrs(addrs);

	return network_interfaces;
}
