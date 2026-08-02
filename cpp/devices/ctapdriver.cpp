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
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include "ctapdriver.h"
#include <spdlog/spdlog.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <iomanip>
#include <sstream>

#ifdef __linux__
#include <linux/if_tun.h>
#include <linux/sockios.h>
#endif

using namespace std;
using namespace piscsi_util;
using namespace network_util;

const string CTapDriver::BRIDGE_NAME = "piscsi_bridge";

namespace {

#ifdef __linux__
string FormatMac(const array<uint8_t, 6>& mac)
{
	ostringstream stream;
	stream << hex << setfill('0');
	for (size_t index = 0; index < mac.size(); index++) {
		if (index) {
			stream << ':';
		}
		stream << setw(2) << static_cast<int>(mac[index]);
	}
	return stream.str();
}
#endif

}

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
	const auto mode = const_params.contains("mode") ? const_params.at("mode") : "";
	const auto interface = const_params.contains("interface") ? const_params.at("interface") : "";
	if (const string error = GetProfileValidationError(mode, interface, GetNetworkInterfaceInfo()); !error.empty()) {
		spdlog::error(error);
		return false;
	}
	if (mode == "proxyarp") {
		const auto configured_uplink = GetConfiguredProxyArpUplink();
		if (!configured_uplink) {
			spdlog::error("PiSCSI proxy-ARP configuration is missing or invalid: /etc/piscsi/network.conf");
			return false;
		}
		if (*configured_uplink != interface) {
			spdlog::error("DaynaPort proxy-ARP interface '" + interface + "' does not match configured uplink '" +
					*configured_uplink + "'");
			return false;
		}
	}

	if (m_hTAP != -1) {
		spdlog::error("DaynaPort TAP device is already active");
		return false;
	}
	const auto interface_mac = GetMacAddress(interface);
	if (interface_mac.size() != m_MacAddr.size()) {
		spdlog::error("Can't determine the MAC address of DaynaPort network interface '" + interface + "'");
		return false;
	}
	m_MacAddr = DeriveDaynaPortMac(interface_mac);

	if (!CreateTap()) {
		return false;
	}

	if (const string error = SetTapUp(); !error.empty()) {
		spdlog::error(error);
		ReleaseTap();
		return false;
	}

	if (mode == DEFAULT_MODE) {
		if (const string error = AttachTapToBridge(); !error.empty()) {
			spdlog::error(error);
			ReleaseTap();
			return false;
		}
	}

	spdlog::info("Tap device piscsi0 created for DaynaPort " + mode + " mode (TAP MAC " +
			FormatMac(m_TapMac) + ", DaynaPort MAC " + FormatMac(m_MacAddr) + ")");

	return true;
#endif
}

string CTapDriver::GetProfileValidationError(const string& mode, const string& interface,
		const network_interface_map& interfaces)
{
	if (mode != DEFAULT_MODE && mode != "proxyarp") {
		return "Unsupported DaynaPort network mode '" + mode + "'";
	}
	if (interfaces.contains("piscsi0")) {
		return "DaynaPort TAP device piscsi0 is already in use";
	}
	if (!IsValidInterfaceName(interface)) {
		return "Invalid DaynaPort network interface '" + interface + "'";
	}
	if (mode == DEFAULT_MODE && interface != BRIDGE_NAME) {
		return "DaynaPort bridge mode requires the pre-configured " + BRIDGE_NAME + " interface";
	}

	const auto it = interfaces.find(interface);
	if (it == interfaces.end()) {
		return "DaynaPort network interface '" + interface + "' does not exist";
	}
	if (!it->second.up) {
		return "DaynaPort network interface '" + interface + "' is down";
	}
	if (mode == DEFAULT_MODE && it->second.type != NetworkInterfaceType::BRIDGE) {
		return "DaynaPort bridge mode requires " + BRIDGE_NAME + " to be a Linux bridge";
	}
	if (mode == "proxyarp" && it->second.type != NetworkInterfaceType::WIFI) {
		return "DaynaPort proxyarp mode requires an active Wi-Fi interface";
	}

	return "";
}

param_map CTapDriver::GetDefaultParams() const
{
	return {
		{ "interface", BRIDGE_NAME },
		{ "mode", DEFAULT_MODE }
	};
}

bool CTapDriver::CreateTap()
{
#ifdef __linux__
	spdlog::trace("Opening tap device");
	m_hTAP = open("/dev/net/tun", O_RDWR);
	if (m_hTAP < 0) {
		LogErrno("Can't open tun");
		return false;
	}

	ifreq ifr = {};
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, "piscsi0", IFNAMSIZ - 1); //NOSONAR Using strncpy is safe
	if (ioctl(m_hTAP, TUNSETIFF, &ifr) < 0) {
		LogErrno("Can't ioctl TUNSETIFF");
		ReleaseTap();
		return false;
	}

	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(m_hTAP, SIOCGIFHWADDR, &ifr) < 0) {
		LogErrno("Can't ioctl SIOCGIFHWADDR");
		ReleaseTap();
		return false;
	}
	memcpy(m_TapMac.data(), ifr.ifr_hwaddr.sa_data, m_TapMac.size());
	return true;
#else
	return false;
#endif
}

string CTapDriver::SetTapUp() const
{
#ifdef __linux__
	const int ip_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (ip_fd < 0) {
		return "Can't open IP socket";
	}
	const string error = ip_link(ip_fd, "piscsi0", true);
	close(ip_fd);
	return error;
#else
	return "TAP networking requires Linux";
#endif
}

string CTapDriver::AttachTapToBridge()
{
#ifdef __linux__
	const int bridge_socket = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (bridge_socket < 0) {
		return "Can't open bridge socket";
	}

	spdlog::trace(">brctl addif " + BRIDGE_NAME + " piscsi0");
	const string error = br_setif(bridge_socket, BRIDGE_NAME, "piscsi0", true);
	close(bridge_socket);
	if (error.empty()) {
		tap_attached_to_bridge = true;
	}
	return error;
#else
	return "TAP networking requires Linux";
#endif
}

void CTapDriver::CleanUp()
{
	ReleaseTap();
}

void CTapDriver::ReleaseTap()
{
	if (tap_attached_to_bridge) {
		if (const int bridge_socket = socket(AF_LOCAL, SOCK_STREAM, 0); bridge_socket < 0) {
			LogErrno("Can't open bridge socket while releasing TAP");
		}
		else {
			spdlog::trace(">brctl delif " + BRIDGE_NAME + " piscsi0");
			if (const string error = br_setif(bridge_socket, BRIDGE_NAME, "piscsi0", false); !error.empty()) {
				spdlog::warn("Removing piscsi0 from " + BRIDGE_NAME + " failed: " + error);
			}
			close(bridge_socket);
		}
		tap_attached_to_bridge = false;
	}

	if (m_hTAP != -1) {
		close(m_hTAP);
		m_hTAP = -1;
	}
}

string CTapDriver::IpLink(bool enable) const
{
	const int fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		return "Can't open IP socket";
	}
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

array<uint8_t, 6> CTapDriver::DeriveDaynaPortMac(span<const uint8_t> interface_mac)
{
	if (interface_mac.size() != 6) {
		return {};
	}

	// Retain the Dayna OUI and derive the unique suffix from
	// the selected bridge or proxy-ARP uplink.
	return { 0x00, 0x80, 0x19, interface_mac[3], interface_mac[4], interface_mac[5] };
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
