//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "network_util.h"
#include <cstring>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#include <netdb.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string_view>

using namespace std;

namespace {

#ifdef __linux__
bool IsVirtualInterface(const string& name)
{
	const filesystem::path path = filesystem::path("/sys/class/net") / name;
	error_code error;
	return filesystem::weakly_canonical(path, error).string().find("/devices/virtual/net/") != string::npos;
}
#endif

}

bool network_util::IsInterfaceUp(const string& interface)
{
	if (!IsValidInterfaceName(interface)) {
		return false;
	}

	ifreq ifr = {};
    strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1); //NOSONAR Using strncpy is safe
	const int fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP);
	if (fd < 0) {
		return false;
	}

	if (!ioctl(fd, SIOCGIFFLAGS, &ifr) && (ifr.ifr_flags & IFF_UP)) {
	    close(fd);
    	return true;
    }

    close(fd);
    return false;
}

bool network_util::IsValidInterfaceName(const string& interface)
{
	return !interface.empty() && interface.length() < IFNAMSIZ && ranges::all_of(interface, [] (const char c) {
		return isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.';
	});
}

vector<uint8_t> network_util::GetMacAddress(const string& interface)
{
#ifdef __linux__
	if (!IsValidInterfaceName(interface)) {
		return {};
	}

	ifreq ifr = {};
	strncpy(ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1); //NOSONAR Using strncpy is safe
	const int fd = socket(PF_INET6, SOCK_DGRAM, IPPROTO_IP);
	if (fd < 0) {
		return {};
	}

	if (!ioctl(fd, SIOCGIFHWADDR, &ifr)) {
		close(fd);
		return vector<uint8_t>(reinterpret_cast<uint8_t*>(ifr.ifr_hwaddr.sa_data),
			reinterpret_cast<uint8_t*>(ifr.ifr_hwaddr.sa_data) + 6);
	}

	close(fd);
#else
	(void)interface;
#endif

	return {};
}

optional<string> network_util::ParseProxyArpUplink(const string& configuration)
{
	constexpr string_view key = "PISCSI_PROXYARP_UPLINK=";
	optional<string> uplink;
	for (size_t start = 0; start < configuration.size();) {
		const size_t end = configuration.find('\n', start);
		const string_view line(configuration.data() + start, (end == string::npos ? configuration.size() : end) - start);
		if (line.starts_with(key)) {
			if (uplink) {
				return nullopt;
			}
			uplink = line.substr(key.length());
		}
		if (end == string::npos) {
			break;
		}
		start = end + 1;
	}

	if (!uplink || !IsValidInterfaceName(*uplink)) {
		return nullopt;
	}
	return uplink;
}

optional<string> network_util::ReadProxyArpUplinkFile(const filesystem::path& path)
{
	error_code error;
	if (!filesystem::is_regular_file(path, error) || error) {
		return nullopt;
	}

	try {
		ifstream configuration(path);
		if (!configuration) {
			return nullopt;
		}
		return ParseProxyArpUplink({ istreambuf_iterator<char>(configuration), {} });
	}
	catch (const ios_base::failure&) {
		return nullopt;
	}
}

optional<string> network_util::GetConfiguredProxyArpUplink()
{
	return ReadProxyArpUplinkFile("/etc/piscsi/network.conf");
}

network_util::NetworkInterfaceType network_util::ClassifyInterface(const bool wireless, const bool bridge,
		const bool tap, const bool virtual_interface, const string& name)
{
	if (name == "lo") {
		return NetworkInterfaceType::LOOPBACK;
	}
	if (bridge) {
		return NetworkInterfaceType::BRIDGE;
	}
	if (wireless) {
		return NetworkInterfaceType::WIFI;
	}
	if (tap) {
		return NetworkInterfaceType::TAP;
	}
	if (virtual_interface) {
		return NetworkInterfaceType::VIRTUAL;
	}
	return NetworkInterfaceType::ETHERNET;
}

network_util::network_interface_map network_util::GetNetworkInterfaceInfo()
{
	network_interface_map interfaces;

#ifdef __linux__
	ifaddrs *addrs = nullptr;
	if (getifaddrs(&addrs) != 0) {
		return interfaces;
	}

	for (ifaddrs *entry = addrs; entry != nullptr; entry = entry->ifa_next) {
		if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_PACKET) {
			continue;
		}

		const string name = entry->ifa_name;
		if (!IsValidInterfaceName(name) || interfaces.contains(name)) {
			continue;
		}

		const filesystem::path path = filesystem::path("/sys/class/net") / name;
		const bool bridge = filesystem::is_directory(path / "bridge");
		const bool wireless = filesystem::is_directory(path / "wireless");
		const bool tap = filesystem::exists(path / "tun_flags");
		interfaces.emplace(name, NetworkInterfaceInfo {
			.name = name,
			.type = ClassifyInterface(wireless, bridge, tap, IsVirtualInterface(name), name),
			.up = (entry->ifa_flags & IFF_UP) != 0,
		});
	}

	freeifaddrs(addrs);
#endif

	return interfaces;
}

set<string, less<>> network_util::GetNetworkInterfaces()
{
	set<string, less<>> network_interfaces;

	for (const auto& [name, interface] : GetNetworkInterfaceInfo()) {
		if (interface.up && ((interface.type == NetworkInterfaceType::BRIDGE && name == "piscsi_bridge") ||
				interface.type == NetworkInterfaceType::ETHERNET || interface.type == NetworkInterfaceType::WIFI)) {
			network_interfaces.insert(name);
		}
	}

	return network_interfaces;
}

bool network_util::ResolveHostName(const string& host, sockaddr_in *addr)
{
	addrinfo hints = {};
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (addrinfo *result; !getaddrinfo(host.c_str(), nullptr, &hints, &result)) {
		*addr = *reinterpret_cast<sockaddr_in *>(result->ai_addr); //NOSONAR bit_cast is not supported by the bullseye compiler
		freeaddrinfo(result);
		return true;
	}

	return false;
}
