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
#include "shared/piscsi_exceptions.h"
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include "ctapdriver.h"
#include <spdlog/spdlog.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sstream>

#ifdef __linux__
#include <sys/epoll.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <linux/sockios.h>
#endif

using namespace std;
using namespace piscsi_util;

const string CTapDriver::BRIDGE_NAME = "piscsi_bridge";

static string br_setif(int br_socket_fd, const char* bridgename, const char* ifname, bool add) {
#ifndef __linux__
	return "Linux is required";
#else
	ifreq ifr;
	ifr.ifr_ifindex = if_nametoindex(ifname);
	if (ifr.ifr_ifindex == 0) {
		return "Can't if_nametoindex " + string(ifname);
	}
	strncpy(ifr.ifr_name, bridgename, IFNAMSIZ - 1);
	if (ioctl(br_socket_fd, add ? SIOCBRADDIF : SIOCBRDELIF, &ifr) < 0) {
		return "Can't ioctl " + string(add ? "SIOCBRADDIF" : "SIOCBRDELIF");
	}
	return "";
#endif
}

CTapDriver::~CTapDriver()
{
	if (m_hTAP != -1) {
		if (const int br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0); br_socket_fd < 0) {
			LogErrno("Can't open bridge socket");
		} else {
			spdlog::trace("brctl delif " + BRIDGE_NAME + " piscsi0");
			if (const string error = br_setif(br_socket_fd, BRIDGE_NAME.c_str(), "piscsi0", false); !error.empty()) {
				spdlog::warn("Warning: Removing piscsi0 from the bridge failed: " + error);
				spdlog::warn("You may need to manually remove the piscsi0 tap device from the bridge");
			}
			close(br_socket_fd);
		}

		// Release TAP device
		close(m_hTAP);
	}
}

string ip_link(int fd, const char* ifname, bool up) {
#ifndef __linux__
	return "Linux is required";
#else
	ifreq ifr;
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1); // Need to save room for null terminator
	int err = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (err) {
		return "Can't ioctl SIOCGIFFLAGS";
	}
	ifr.ifr_flags &= ~IFF_UP;
	if (up) {
		ifr.ifr_flags |= IFF_UP;
	}
	err = ioctl(fd, SIOCSIFFLAGS, &ifr);
	if (err) {
		return "Can't ioctl SIOCSIFFLAGS";
	}
	return "";
#endif
}

static bool is_interface_up(string_view interface) {
	string file = "/sys/class/net/";
	file += interface;
	file += "/carrier";

	bool status = true;
	FILE *fp = fopen(file.c_str(), "r");
	if (!fp || fgetc(fp) != '1') {
		status = false;
	}

	if (fp) {
		fclose(fp);
	}

	return status;
}

bool CTapDriver::Init(const unordered_map<string, string>& const_params)
{
#ifndef __linux__
	return false;
#else
	unordered_map<string, string> params = const_params;
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

	spdlog::trace("Opened tap device " + to_string(m_hTAP));

	// IFF_NO_PI for no extra packet information
	ifreq ifr = {};
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, "piscsi0", IFNAMSIZ - 1);

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
		if (!empty(error)) {
			LogErrno(error);
		}
		close(m_hTAP);
		close(ip_fd);
		close(br_socket_fd);
		return false;
	};

	// Check if the bridge has already been created
	string sys_file = "/sys/class/net/";
	sys_file += BRIDGE_NAME;
	if (access(sys_file.c_str(), F_OK)) {
		spdlog::info(BRIDGE_NAME + " is not yet available");

		spdlog::trace("Checking which interface is available for creating the bridge");

		string bridge_interface;
		for (const string& iface : interfaces) {
			if (is_interface_up(iface)) {
				spdlog::trace("Interface " + iface + " is up");

				bridge_interface = iface;
				break;
			}
			else {
				spdlog::trace("Interface " + iface + " is not available or is not up");
			}
		}

		if (bridge_interface.empty()) {
			spdlog::error("No interface is up, not creating bridge");
			return false;
		}

		spdlog::info("Creating " + BRIDGE_NAME + " for interface " + bridge_interface);

		if (bridge_interface == "eth0") {
			spdlog::trace("brctl addbr " + BRIDGE_NAME);

			if (ioctl(br_socket_fd, SIOCBRADDBR, BRIDGE_NAME.c_str()) < 0) {
				return cleanUp("Can't ioctl SIOCBRADDBR");
			}

			spdlog::trace("brctl addif " + BRIDGE_NAME + " " + bridge_interface);

			if (const string error = br_setif(br_socket_fd, BRIDGE_NAME.c_str(), bridge_interface.c_str(), true); !error.empty()) {
				return cleanUp(error);
			}
		}
		else {
			string address = inet;
			string netmask = "255.255.255.0"; //NOSONAR This hardcoded IP address is safe
			if (size_t separatorPos = inet.find('/'); separatorPos != string::npos) {
				address = inet.substr(0, separatorPos);

				int m;
				if (!GetAsUnsignedInt(inet.substr(separatorPos + 1), m) || m < 8 || m > 32) {
					return cleanUp("Invalid CIDR netmask notation '" + inet.substr(separatorPos + 1) + "'");
				}

				// long long is required for compatibility with 32 bit platforms
				const auto mask = (long long)(pow(2, 32) - (1 << (32 - m)));
				netmask = to_string((mask >> 24) & 0xff) + '.' + to_string((mask >> 16) & 0xff) + '.' +
						to_string((mask >> 8) & 0xff) + '.' + to_string(mask & 0xff);
			}

			spdlog::trace("brctl addbr " + BRIDGE_NAME);

			if (ioctl(br_socket_fd, SIOCBRADDBR, BRIDGE_NAME.c_str()) < 0) {
				return cleanUp("Can't ioctl SIOCBRADDBR");
			}

			ifreq ifr_a;
			ifr_a.ifr_addr.sa_family = AF_INET;
			strncpy(ifr_a.ifr_name, BRIDGE_NAME.c_str(), IFNAMSIZ);
			if (auto addr = (sockaddr_in*)&ifr_a.ifr_addr;
				inet_pton(AF_INET, address.c_str(), &addr->sin_addr) != 1) {
				return cleanUp("Can't convert '" + address + "' into a network address");
			}

			ifreq ifr_n;
			ifr_n.ifr_addr.sa_family = AF_INET;
			strncpy(ifr_n.ifr_name, BRIDGE_NAME.c_str(), IFNAMSIZ);
			if (auto mask = (sockaddr_in*)&ifr_n.ifr_addr;
				inet_pton(AF_INET, netmask.c_str(), &mask->sin_addr) != 1) {
				return cleanUp("Can't convert '" + netmask + "' into a netmask");
			}

			spdlog::trace("ip address add " + inet + " dev " + BRIDGE_NAME);

			if (ioctl(ip_fd, SIOCSIFADDR, &ifr_a) < 0 || ioctl(ip_fd, SIOCSIFNETMASK, &ifr_n) < 0) {
				return cleanUp("Can't ioctl SIOCSIFADDR or SIOCSIFNETMASK");
			}
		}

		spdlog::trace("ip link set dev " + BRIDGE_NAME + " up");

		if (const string error = ip_link(ip_fd, BRIDGE_NAME.c_str(), true); !error.empty()) {
			return cleanUp(error);
		}
	}
	else
	{
		spdlog::info(BRIDGE_NAME + " is already available");
	}

	spdlog::trace("ip link set piscsi0 up");

	if (const string error = ip_link(ip_fd, "piscsi0", true); !error.empty()) {
		return cleanUp(error);
	}

	spdlog::trace("brctl addif " + BRIDGE_NAME + " piscsi0");

	if (const string error = br_setif(br_socket_fd, BRIDGE_NAME.c_str(), "piscsi0", true); !error.empty()) {
		return cleanUp(error);
	}

	// Get MAC address
	spdlog::trace("Getting the MAC address");

	ifr.ifr_addr.sa_family = AF_INET;
	if (ioctl(m_hTAP, SIOCGIFHWADDR, &ifr) < 0) {
		return cleanUp("Can't ioctl SIOCGIFHWADDR");
	}
	spdlog::trace("Got the MAC");

	// Save MAC address
	memcpy(m_MacAddr.data(), ifr.ifr_hwaddr.sa_data, m_MacAddr.size());

	close(ip_fd);
	close(br_socket_fd);

	spdlog::info("Tap device " + string(ifr.ifr_name) + " created");

	return true;
#endif
}

string CTapDriver::Enable() const
{
	const int fd = socket(PF_INET, SOCK_DGRAM, 0);
	spdlog::trace("ip link set piscsi0 up");
	const string result = ip_link(fd, "piscsi0", true);
	close(fd);
	return result;
}

string CTapDriver::Disable() const
{
	const int fd = socket(PF_INET, SOCK_DGRAM, 0);
	spdlog::trace("ip link set piscsi0 down");
	const string result = ip_link(fd, "piscsi0", false);
	close(fd);
	return result;
}

void CTapDriver::Flush() const
{
	while (PendingPackets()) {
		array<uint8_t, ETH_FRAME_LEN> m_garbage_buffer;
		(void)Receive(m_garbage_buffer.data());
	}
}

void CTapDriver::GetMacAddr(uint8_t *mac) const
{
	assert(mac);

	memcpy(mac, m_MacAddr.data(), m_MacAddr.size());
}

bool CTapDriver::PendingPackets() const
{
	assert(m_hTAP != -1);

	// Check if there is data that can be received
	pollfd fds;
	fds.fd = m_hTAP;
	fds.events = POLLIN | POLLERR;
	fds.revents = 0;
	poll(&fds, 1, 0);
	spdlog::trace(to_string(fds.revents) + " revents");
	return !(fds.revents & POLLIN);
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
	if (!PendingPackets()) {
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
