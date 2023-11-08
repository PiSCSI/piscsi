//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	Copyright (C) akuker
//
//---------------------------------------------------------------------------

#include "shared/piscsi_util.h"
#include "shared/network_util.h"
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include "ctapdriver.h"
#include <spdlog/spdlog.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sstream>

#ifdef __linux__
#include <linux/if_tun.h>
#include <linux/sockios.h>
#endif

using namespace std;
using namespace piscsi_util;
using namespace network_util;

const string CTapDriver::BRIDGE_NAME = "piscsi_bridge";

static string br_setif(int br_socket_fd, const string& bridgename, const string& ifname, bool add) {
#ifndef __linux__
	return "if_nametoindex: Linux is required";
#else
	ifreq ifr;
	ifr.ifr_ifindex = if_nametoindex(ifname.c_str());
	if (ifr.ifr_ifindex == 0) {
		return "Can't if_nametoindex " + ifname;
	}
	strncpy(ifr.ifr_name, bridgename.c_str(), IFNAMSIZ - 1); //NOSONAR Using strncpy is safe
	if (ioctl(br_socket_fd, add ? SIOCBRADDIF : SIOCBRDELIF, &ifr) < 0) {
		return "Can't ioctl " + string(add ? "SIOCBRADDIF" : "SIOCBRDELIF");
	}
	return "";
#endif
}

string ip_link(int fd, const char* ifname, bool up) {
#ifndef __linux__
	return "Can't ip_link: Linux is required";
#else
	ifreq ifr;
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1); //NOSONAR Using strncpy is safe
	if (ioctl(fd, SIOCGIFFLAGS, &ifr)) {
		return "Can't ioctl SIOCGIFFLAGS";
	}
	ifr.ifr_flags &= ~IFF_UP;
	if (up) {
		ifr.ifr_flags |= IFF_UP;
	}
	if (ioctl(fd, SIOCSIFFLAGS, &ifr)) {
		return "Can't ioctl SIOCSIFFLAGS";
	}
	return "";
#endif
}

bool CTapDriver::Init(const param_map& const_params)
{
#ifndef __linux__
	return false;
#else
	param_map params = const_params;
	stringstream s(params["interface"]);
	string interface;
	while (getline(s, interface, ',')) {
		interfaces.push_back(interface);
	}
	inet = params["inet"];

	spdlog::trace("Opening tap device");
	// TAP device initilization
	if ((m_hTAP = open("/dev/net/tun", O_RDWR)) < 0) {
		LogErrno("Can't open tun");
		return false;
	}

	// IFF_NO_PI for no extra packet information
	ifreq ifr = {};
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, "piscsi0", IFNAMSIZ - 1); //NOSONAR Using strncpy is safe

	spdlog::trace("Going to open " + string(ifr.ifr_name));

	const int ret = ioctl(m_hTAP, TUNSETIFF, (void *)&ifr);
	if (ret < 0) {
		LogErrno("Can't ioctl TUNSETIFF");

		close(m_hTAP);
		return false;
	}

	spdlog::trace("Return code from ioctl was " + to_string(ret));

	const int ip_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (ip_fd < 0) {
		LogErrno("Can't open ip socket");

		close(m_hTAP);
		return false;
	}

	const int br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (br_socket_fd < 0) {
		LogErrno("Can't open bridge socket");

		close(m_hTAP);
		close(ip_fd);
		return false;
	}

	auto cleanUp = [&] (const string& error) {
		LogErrno(error);
		close(m_hTAP);
		close(ip_fd);
		close(br_socket_fd);
		return false;
	};

	// Check if the bridge has already been created
	// TODO Find an alternative to accessing a file, there is most likely a system call/ioctl
	if (access(string("/sys/class/net/" + BRIDGE_NAME).c_str(), F_OK)) {
		spdlog::trace("Checking which interface is available for creating the bridge " + BRIDGE_NAME);

		const auto& it = ranges::find_if(interfaces, [] (const string& iface) { return IsInterfaceUp(iface); } );
		if (it == interfaces.end()) {
			return cleanUp("No interface is up, not creating bridge " + BRIDGE_NAME);
		}

		const string bridge_interface = *it;

		spdlog::info("Creating " + BRIDGE_NAME + " for interface " + bridge_interface);

		if (bridge_interface == "eth0") {
			if (const string error = SetUpEth0(br_socket_fd, bridge_interface); !error.empty()) {
				return cleanUp(error);
			}
		}
		else if (const string error = SetUpNonEth0(br_socket_fd, ip_fd, inet); !error.empty()) {
			return cleanUp(error);
		}

		spdlog::trace(">ip link set dev " + BRIDGE_NAME + " up");

		if (const string error = ip_link(ip_fd, BRIDGE_NAME.c_str(), true); !error.empty()) {
			return cleanUp(error);
		}
	}
	else {
		spdlog::info(BRIDGE_NAME + " is already available");
	}

	spdlog::trace(">ip link set piscsi0 up");

	if (const string error = ip_link(ip_fd, "piscsi0", true); !error.empty()) {
		return cleanUp(error);
	}

	spdlog::trace(">brctl addif " + BRIDGE_NAME + " piscsi0");

	if (const string error = br_setif(br_socket_fd, BRIDGE_NAME, "piscsi0", true); !error.empty()) {
		return cleanUp(error);
	}

	spdlog::trace("Getting the MAC address");

	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(m_hTAP, SIOCGIFHWADDR, &ifr) < 0) {
		return cleanUp("Can't ioctl SIOCGIFHWADDR");
	}

	// Save MAC address
	memcpy(m_MacAddr.data(), ifr.ifr_hwaddr.sa_data, m_MacAddr.size());

	close(ip_fd);
	close(br_socket_fd);

	spdlog::info("Tap device " + string(ifr.ifr_name) + " created");

	return true;
#endif
}

void CTapDriver::CleanUp() const
{
	if (m_hTAP != -1) {
		if (const int br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0); br_socket_fd < 0) {
			LogErrno("Can't open bridge socket");
		} else {
			spdlog::trace(">brctl delif " + BRIDGE_NAME + " piscsi0");
			if (const string error = br_setif(br_socket_fd, BRIDGE_NAME, "piscsi0", false); !error.empty()) {
				spdlog::warn("Warning: Removing piscsi0 from the bridge failed: " + error);
				spdlog::warn("You may need to manually remove the piscsi0 tap device from the bridge");
			}
			close(br_socket_fd);
		}

		// Release TAP device
		close(m_hTAP);
	}
}

param_map CTapDriver::GetDefaultParams() const
{
	return {
		{ "interface", Join(GetNetworkInterfaces(), ",") },
		{ "inet", DEFAULT_IP }
	};
}

pair<string, string> CTapDriver::ExtractAddressAndMask(const string& s)
{
	string address = s;
	string netmask = "255.255.255.0"; //NOSONAR This hardcoded IP address is safe
	if (const auto& components = Split(s, '/', 2); components.size() == 2) {
		address = components[0];

		int m;
		if (!GetAsUnsignedInt(components[1], m) || m < 8 || m > 32) {
			spdlog::error("Invalid CIDR netmask notation '" + components[1] + "'");
			return { "", "" };
		}

		// long long is required for compatibility with 32 bit platforms
		const auto mask = (long long)(pow(2, 32) - (1 << (32 - m)));
		netmask = to_string((mask >> 24) & 0xff) + '.' + to_string((mask >> 16) & 0xff) + '.' +
				to_string((mask >> 8) & 0xff) + '.' + to_string(mask & 0xff);
	}

	return { address, netmask };
}

string CTapDriver::SetUpEth0(int socket_fd, const string& bridge_interface)
{
#ifdef __linux__
	spdlog::trace(">brctl addbr " + BRIDGE_NAME);

	if (ioctl(socket_fd, SIOCBRADDBR, BRIDGE_NAME.c_str()) < 0) {
		return "Can't ioctl SIOCBRADDBR";
	}

	spdlog::trace(">brctl addif " + BRIDGE_NAME + " " + bridge_interface);

	if (const string error = br_setif(socket_fd, BRIDGE_NAME, bridge_interface, true); !error.empty()) {
		return error;
	}
#endif

	return "";
}

string CTapDriver::SetUpNonEth0(int socket_fd, int ip_fd, const string& s)
{
#ifdef __linux__
	const auto [address, netmask] = ExtractAddressAndMask(s);
	if (address.empty() || netmask.empty()) {
		return "Error extracting inet address and netmask";
	}

	spdlog::trace(">brctl addbr " + BRIDGE_NAME);

	if (ioctl(socket_fd, SIOCBRADDBR, BRIDGE_NAME.c_str()) < 0) {
		return "Can't ioctl SIOCBRADDBR";
	}

	ifreq ifr_a;
	ifr_a.ifr_addr.sa_family = AF_INET;
	strncpy(ifr_a.ifr_name, BRIDGE_NAME.c_str(), IFNAMSIZ - 1); //NOSONAR Using strncpy is safe
	if (auto addr = (sockaddr_in*)&ifr_a.ifr_addr;
		inet_pton(AF_INET, address.c_str(), &addr->sin_addr) != 1) {
		return "Can't convert '" + address + "' into a network address";
	}

	ifreq ifr_n;
	ifr_n.ifr_addr.sa_family = AF_INET;
	strncpy(ifr_n.ifr_name, BRIDGE_NAME.c_str(), IFNAMSIZ - 1); //NOSONAR Using strncpy is safe
	if (auto mask = (sockaddr_in*)&ifr_n.ifr_addr;
		inet_pton(AF_INET, netmask.c_str(), &mask->sin_addr) != 1) {
		return "Can't convert '" + netmask + "' into a netmask";
	}

	spdlog::trace(">ip address add " + s + " dev " + BRIDGE_NAME);

	if (ioctl(ip_fd, SIOCSIFADDR, &ifr_a) < 0 || ioctl(ip_fd, SIOCSIFNETMASK, &ifr_n) < 0) {
		return "Can't ioctl SIOCSIFADDR or SIOCSIFNETMASK";
	}
#endif

	return "";
}

string CTapDriver::IpLink(bool enable) const
{
	const int fd = socket(PF_INET, SOCK_DGRAM, 0);
	spdlog::trace(string(">ip link set piscsi0 ") + (enable ? "up" : "down"));
	const string result = ip_link(fd, "piscsi0", enable);
	close(fd);
	return result;
}

void CTapDriver::Flush() const
{
	while (HasPendingPackets()) {
		array<uint8_t, ETH_FRAME_LEN> m_garbage_buffer;
		(void)Receive(m_garbage_buffer.data());
	}
}

void CTapDriver::GetMacAddr(uint8_t *mac) const
{
	assert(mac);

	memcpy(mac, m_MacAddr.data(), m_MacAddr.size());
}

bool CTapDriver::HasPendingPackets() const
{
	assert(m_hTAP != -1);

	// Check if there is data that can be received
	pollfd fds;
	fds.fd = m_hTAP;
	fds.events = POLLIN | POLLERR;
	fds.revents = 0;
	poll(&fds, 1, 0);
	spdlog::trace(to_string(fds.revents) + " revents");
	return fds.revents & POLLIN;
}

// See https://stackoverflow.com/questions/21001659/crc32-algorithm-implementation-in-c-without-a-look-up-table-and-with-a-public-li
uint32_t CTapDriver::Crc32(span<const uint8_t> data) {
   uint32_t crc = 0xffffffff;
   for (const auto d: data) {
      crc ^= d;
      for (int i = 0; i < 8; i++) {
         const uint32_t mask = -(static_cast<int>(crc) & 1);
         crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
   }
   return ~crc;
}

int CTapDriver::Receive(uint8_t *buf) const
{
	assert(m_hTAP != -1);

	// Check if there is data that can be received
	if (!HasPendingPackets()) {
		return 0;
	}

	// Receive
	auto dwReceived = static_cast<uint32_t>(read(m_hTAP, buf, ETH_FRAME_LEN));
	if (dwReceived == static_cast<uint32_t>(-1)) {
		spdlog::warn("Error occured while receiving a packet");
		return 0;
	}

	// If reception is enabled
	if (dwReceived > 0) {
		// We need to add the Frame Check Status (FCS) CRC back onto the end of the packet.
		// The Linux network subsystem removes it, since most software apps shouldn't ever
		// need it.
		const int crc = Crc32(span(buf, dwReceived));

		buf[dwReceived + 0] = (uint8_t)((crc >> 0) & 0xFF);
		buf[dwReceived + 1] = (uint8_t)((crc >> 8) & 0xFF);
		buf[dwReceived + 2] = (uint8_t)((crc >> 16) & 0xFF);
		buf[dwReceived + 3] = (uint8_t)((crc >> 24) & 0xFF);

		spdlog::trace("CRC is " + to_string(crc) + " - " + to_string(buf[dwReceived+0]) + " " + to_string(buf[dwReceived+1]) +
				" " + to_string(buf[dwReceived+2]) + " " + to_string(buf[dwReceived+3]));

		// Add FCS size to the received message size
		dwReceived += 4;
	}

	// Return the number of bytes
	return dwReceived;
}

int CTapDriver::Send(const uint8_t *buf, int len) const
{
	assert(m_hTAP != -1);

	// Start sending
	return static_cast<int>(write(m_hTAP, buf, len));
}
