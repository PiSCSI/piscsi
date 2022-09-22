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

multimap<int, unique_ptr<PrimaryDevice>> DeviceFactory::devices;

DeviceFactory::DeviceFactory()
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

	extension_mapping["hd1"] = SCHD;
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

void DeviceFactory::DeleteDevice(const PrimaryDevice& device) const
{
	auto [begin, end] = devices.equal_range(device.GetId());
	for (auto& it = begin; it != end; ++it) {
		if (it->second->GetLun() == device.GetLun()) {
			devices.erase(it);

			break;
		}
	}
}

void DeviceFactory::DeleteAllDevices() const
{
	devices.clear();
}

const PrimaryDevice *DeviceFactory::GetDeviceByIdAndLun(int i, int lun) const
{
	for (const auto& [id, device] : devices) {
		if (device->GetId() == i && device->GetLun() == lun) {
			return device.get();
		}
	}

	return nullptr;
}

list<PrimaryDevice *> DeviceFactory::GetAllDevices() const
{
	list<PrimaryDevice *> result;

	for (const auto& [id, device] : devices) {
		result.push_back(device.get());
	}

	return result;
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
PrimaryDevice *DeviceFactory::CreateDevice(PbDeviceType type, const string& filename, int id)
{
	// If no type was specified try to derive the device type from the filename
	if (type == UNDEFINED) {
		type = GetTypeForFile(filename);
		if (type == UNDEFINED) {
			return nullptr;
		}
	}

	unique_ptr<PrimaryDevice> device;
	switch (type) {
	case SCHD: {
		if (string ext = GetExtension(filename); ext == "hdn" || ext == "hdi" || ext == "nhd") {
			device = make_unique<SCSIHD_NEC>();
		} else {
			device = make_unique<SCSIHD>(sector_sizes[SCHD], false, ext == "hd1" ? scsi_level::SCSI_1_CCS : scsi_level::SCSI_2);

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
		device = make_unique<SCSIHD>(sector_sizes[SCRM], true);
		device->SetProtectable(true);
		device->SetStoppable(true);
		device->SetRemovable(true);
		device->SetLockable(true);
		device->SetProduct("SCSI HD (REM.)");
		break;

	case SCMO:
		device = make_unique<SCSIMO>(sector_sizes[SCMO], geometries[SCMO]);
		device->SetProtectable(true);
		device->SetStoppable(true);
		device->SetRemovable(true);
		device->SetLockable(true);
		device->SetProduct("SCSI MO");
		break;

	case SCCD:
		device = make_unique<SCSICD>(sector_sizes[SCCD]);
		device->SetReadOnly(true);
		device->SetStoppable(true);
		device->SetRemovable(true);
		device->SetLockable(true);
		device->SetProduct("SCSI CD-ROM");
		break;

	case SCBR:
		device = make_unique<SCSIBR>();
		device->SetProduct("SCSI HOST BRIDGE");
		device->SupportsParams(true);
		device->SetDefaultParams(default_params[SCBR]);
		break;

	case SCDP:
		device = make_unique<SCSIDaynaPort>();
		// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
		device->SetVendor("Dayna");
		device->SetProduct("SCSI/Link");
		device->SetRevision("1.4a");
		device->SupportsParams(true);
		device->SetDefaultParams(default_params[SCDP]);
		break;

	case SCHS:
		device = make_unique<HostServices>(this);
		// Since this is an emulation for a specific device the full INQUIRY data have to be set accordingly
		device->SetVendor("RaSCSI");
		device->SetProduct("Host Services");
		break;

	case SCLP:
		device = make_unique<SCSIPrinter>();
		device->SetProduct("SCSI PRINTER");
		device->SupportsParams(true);
		device->SetDefaultParams(default_params[SCLP]);
		break;

	default:
		break;
	}

	assert(device != nullptr);

	PrimaryDevice *d = device.release();

	if (d != nullptr) {
		d->SetId(id);

		devices.emplace(id, d);
	}

	return d;
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

#ifdef __linux
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
