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
#include "exceptions.h"
#include "device_factory.h"
#include <ifaddrs.h>
#include "host_services.h"

using namespace std;
using namespace rascsi_interface;

multimap<int, Device *> DeviceFactory::devices;

DeviceFactory::DeviceFactory() : sector_sizes({}), geometries({}), default_params({}), extension_mapping({})
{
	sector_sizes[SCHD] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCRM] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCMO] = { 512, 1024, 2048, 4096 };
	sector_sizes[SCCD] = { 512, 2048};

	// 128 MB, 512 bytes per sector, 248826 sectors
	geometries[SCMO][0x797f400] = make_pair(512, 248826);
	// 230 MB, 512 bytes per block, 446325 sectors
	geometries[SCMO][0xd9eea00] = make_pair(512, 446325);
	// 540 MB, 512 bytes per sector, 1041500 sectors
	geometries[SCMO][0x1fc8b800] = make_pair(512, 1041500);
	// 640 MB, 20248 bytes per sector, 310352 sectors
	geometries[SCMO][0x25e28000] = make_pair(2048, 310352);

	string network_interfaces;
	for (const auto& network_interface : GetNetworkInterfaces()) {
		if (!network_interfaces.empty()) {
			network_interfaces += ",";
		}
		network_interfaces += network_interface;
	}

	default_params[SCBR]["interface"] = network_interfaces;
	default_params[SCBR]["inet"] = "10.10.20.1/24";
	default_params[SCDP]["interface"] = network_interfaces;
	default_params[SCDP]["inet"] = "10.10.20.1/24";
	default_params[SCLP]["cmd"] = "lp -oraw %f";
	default_params[SCLP]["timeout"] = "30";

	extension_mapping["hds"] = SCHD;
	extension_mapping["hda"] = SCHD;
	extension_mapping["hdn"] = SCHD;
	extension_mapping["hdi"] = SCHD;
	extension_mapping["nhd"] = SCHD;
	extension_mapping["hdr"] = SCRM;
	extension_mapping["mos"] = SCMO;
	extension_mapping["iso"] = SCCD;
}

DeviceFactory::~DeviceFactory()
{
	DeleteAllDevices();
}

DeviceFactory& DeviceFactory::instance()
{
	static DeviceFactory instance;
	return instance;
}

void DeviceFactory::DeleteDevice(Device *device)
{
	auto iterpair = devices.equal_range(device->GetId());

	for (auto it = iterpair.first; it != iterpair.second; ++it) {
		if (it->second->GetLun() == device->GetLun()) {
			devices.erase(it);
			delete device;

			break;
		}
	}
}

void DeviceFactory::DeleteAllDevices()
{
	for (const auto& device : devices) {
		delete device.second;
	}

	devices.clear();
}

const Device * DeviceFactory::GetDeviceByIdAndLun(int id, int lun) const
{
	for (const auto& device : devices) {
		if (device.second->GetId() == id && device.second->GetLun() == lun) {
			return device.second;
		}
	}

	return nullptr;
}

const list<Device *> DeviceFactory::GetAllDevices() const
{
	list<Device *> result;

	for (const auto& device : devices) {
		result.push_back(device.second);
	}

	return result;
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

PbDeviceType DeviceFactory::GetTypeForFile(const string& filename) const
{
	string ext = GetExtension(filename);

	const auto& it = extension_mapping.find(ext);
	if (it != extension_mapping.end()) {
		return it->second;
	}
	else if (filename == "bridge") {
		return SCBR;
	}
	else if (filename == "daynaport") {
		return SCDP;
	}
	else if (filename == "printer") {
		return SCLP;
	}
	else if (filename == "services") {
		return SCHS;
	}

	return UNDEFINED;
}

// ID -1 is used by rascsi to create a temporary device
Device *DeviceFactory::CreateDevice(PbDeviceType type, const string& filename, int id)
{
	// If no type was specified try to derive the device type from the filename
	if (type == UNDEFINED) {
		type = GetTypeForFile(filename);
		if (type == UNDEFINED) {
			return nullptr;
		}
	}

	Device *device = nullptr;
	try {
		switch (type) {
			case SCHD: {
				string ext = GetExtension(filename);
				if (ext == "hdn" || ext == "hdi" || ext == "nhd") {
					device = new SCSIHD_NEC({ 512 });
				} else {
					device = new SCSIHD(sector_sizes[SCHD], false);

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
				device = new SCSIHD(sector_sizes[SCRM], true);
				device->SetProtectable(true);
				device->SetStoppable(true);
				device->SetRemovable(true);
				device->SetLockable(true);
				device->SetProduct("SCSI HD (REM.)");
				break;

			case SCMO:
				device = new SCSIMO(sector_sizes[SCMO], geometries[SCMO]);
				device->SetProtectable(true);
				device->SetStoppable(true);
				device->SetRemovable(true);
				device->SetLockable(true);
				device->SetProduct("SCSI MO");
				break;

			case SCCD:
				device = new SCSICD(sector_sizes[SCCD]);
				device->SetReadOnly(true);
				device->SetStoppable(true);
				device->SetRemovable(true);
				device->SetLockable(true);
				device->SetProduct("SCSI CD-ROM");
				break;

			case SCBR:
				device = new SCSIBR();
				device->SetProduct("SCSI HOST BRIDGE");
				device->SupportsParams(true);
				device->SetDefaultParams(default_params[SCBR]);
				break;

			case SCDP:
				device = new SCSIDaynaPort();
				// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
				device->SetVendor("Dayna");
				device->SetProduct("SCSI/Link");
				device->SetRevision("1.4a");
				device->SupportsParams(true);
				device->SetDefaultParams(default_params[SCDP]);
				break;

			case SCHS:
				device = new HostServices();
				// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
				device->SetVendor("RaSCSI");
				device->SetProduct("Host Services");
				break;

			case SCLP:
				device = new SCSIPrinter();
				device->SetProduct("SCSI PRINTER");
				device->SupportsParams(true);
				device->SetDefaultParams(default_params[SCLP]);
				break;

			default:
				break;
		}
	}
	catch(const illegal_argument_exception& e) {
		// There was an internal problem with setting up the device data for INQUIRY
		return nullptr;
	}

	device->SetId(id);

	devices.emplace(id, device);

	return device;
}

const unordered_set<uint32_t>& DeviceFactory::GetSectorSizes(const string& type) const
{
	PbDeviceType t = UNDEFINED;
	PbDeviceType_Parse(type, &t);

	const auto it = sector_sizes.find(t);
	if (it == sector_sizes.end()) {
		throw illegal_argument_exception("Sector sizes requested for a device without sectors");
	}

	return it->second;
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
