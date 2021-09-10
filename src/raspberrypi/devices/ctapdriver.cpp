//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	Copyright (C) akuker
//
//	Imported NetBSD support and some optimisation patches by Rin Okuyama.
//
//	[ TAP Driver ]
//
//---------------------------------------------------------------------------

#include <unistd.h>
#include <arpa/inet.h>
#ifdef __linux__
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#endif
#include <zlib.h> // For crc32()
#include "os.h"
#include "../rascsi.h"
#include "ctapdriver.h"
#include "log.h"
#include "exceptions.h"
#include <sstream>

using namespace std;

CTapDriver::CTapDriver(const string& interfaces)
{
	stringstream s(interfaces);
	string interface;
	while (getline(s, interface, ',')) {
		this->interfaces.push_back(interface);
	}

	// Initialization
	m_bTxValid = FALSE;
	m_hTAP = -1;
	memset(&m_MacAddr, 0, sizeof(m_MacAddr));
	m_pcap = NULL;
	m_pcap_dumper = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
#ifdef __linux__

static BOOL br_setif(int br_socket_fd, const char* bridgename, const char* ifname, BOOL add) {
	struct ifreq ifr;
	ifr.ifr_ifindex = if_nametoindex(ifname);
	if (ifr.ifr_ifindex == 0) {
		LOGERROR("Error: can't if_nametoindex. Errno: %d %s", errno, strerror(errno));
		return FALSE;
	}
	strncpy(ifr.ifr_name, bridgename, IFNAMSIZ);
	if (ioctl(br_socket_fd, add ? SIOCBRADDIF : SIOCBRDELIF, &ifr) < 0) {
		LOGERROR("Error: can't ioctl %s. Errno: %d %s", add ? "SIOCBRADDIF" : "SIOCBRDELIF", errno, strerror(errno));
		return FALSE;
	}
	return TRUE;
}

static BOOL ip_link(int fd, const char* ifname, BOOL up) {
	struct ifreq ifr;
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1); // Need to save room for null terminator
	int err = ioctl(fd, SIOCGIFFLAGS, &ifr);
	if (err) {
		LOGERROR("Error: can't ioctl SIOCGIFFLAGS. Errno: %d %s", errno, strerror(errno));
		return FALSE;
	}
	ifr.ifr_flags &= ~IFF_UP;
	if (up) {
		ifr.ifr_flags |= IFF_UP;
	}
	err = ioctl(fd, SIOCSIFFLAGS, &ifr);
	if (err) {
		LOGERROR("Error: can't ioctl SIOCSIFFLAGS. Errno: %d %s", errno, strerror(errno));
		return FALSE;
	}
	return TRUE;
}

static bool is_interface_up(const string& interface) {
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

bool CTapDriver::Init()
{
	LOGTRACE("Opening Tap device");
	// TAP device initilization
	if ((m_hTAP = open("/dev/net/tun", O_RDWR)) < 0) {
		LOGERROR("Error: can't open tun. Errno: %d %s", errno, strerror(errno));
		return false;
	}

	LOGTRACE("Opened tap device %d",m_hTAP);
	
	// IFF_NO_PI for no extra packet information
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	char dev[IFNAMSIZ] = "ras0";
	strncpy(ifr.ifr_name, dev, IFNAMSIZ);

	LOGTRACE("Going to open %s", ifr.ifr_name);

	int ret = ioctl(m_hTAP, TUNSETIFF, (void *)&ifr);
	if (ret < 0) {
		LOGERROR("Error: can't ioctl TUNSETIFF. Errno: %d %s", errno, strerror(errno));

		close(m_hTAP);
		return false;
	}

	LOGTRACE("return code from ioctl was %d", ret);

	int ip_fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (ip_fd < 0) {
		LOGERROR("Error: can't open ip socket. Errno: %d %s", errno, strerror(errno));

		close(m_hTAP);
		return false;
	}

	int br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
	if (br_socket_fd < 0) {
		LOGERROR("Error: can't open bridge socket. Errno: %d %s", errno, strerror(errno));

		close(m_hTAP);
		close(ip_fd);
		return false;
	}

	// Check if the bridge has already been created
	if (access("/sys/class/net/rascsi_bridge", F_OK)) {
		LOGINFO("rascsi_bridge is not yet available");

		LOGTRACE("Checking which interface is available for creating the bridge");

		string bridge_interface;
		string bridge_ip;
		for (const string& iface : interfaces) {
			string interface;
			size_t separatorPos = iface.find(':');
			if (separatorPos != string::npos) {
				interface = iface.substr(0, separatorPos);
				bridge_ip = iface.substr(separatorPos + 1);
			}
			else {
				interface = iface;
				bridge_ip = "10.10.20.1/24";
			}

			ostringstream msg;
			if (is_interface_up(interface)) {
				msg << "Interface " << interface << " is up";
				LOGTRACE("%s", msg.str().c_str());

				bridge_interface = interface;
				break;
			}
			else {
				msg << "Interface " << interface << " is not available or is not up";
				LOGTRACE("%s", msg.str().c_str());
			}
		}

		if (bridge_interface.empty()) {
			LOGERROR("No interface is up, not creating bridge");
			return false;
		}

		LOGINFO("Creating rascsi_bridge for interface %s", bridge_interface.c_str());

		if (bridge_interface == "eth0") {
			LOGDEBUG("brctl addbr rascsi_bridge");

			if ((ret = ioctl(br_socket_fd, SIOCBRADDBR, "rascsi_bridge")) < 0) {
				LOGERROR("Error: can't ioctl SIOCBRADDBR. Errno: %d %s", errno, strerror(errno));

				close(m_hTAP);
				close(ip_fd);
				close(br_socket_fd);
				return false;
			}

			LOGDEBUG("brctl addif rascsi_bridge %s", bridge_interface.c_str());

			if (!br_setif(br_socket_fd, "rascsi_bridge", bridge_interface.c_str(), TRUE)) {
				close(m_hTAP);
				close(ip_fd);
				close(br_socket_fd);
				return false;
			}
		}
		else {
			LOGDEBUG("ip address add %s dev rascsi_bridge", bridge_ip.c_str());

			string address = bridge_ip;
			string netmask = "255.255.255.0";
			size_t separatorPos = bridge_ip.find('/');
			if (separatorPos != string::npos) {
				address = bridge_ip.substr(0, separatorPos);

				string mask = bridge_ip.substr(separatorPos + 1);
				if (mask == "8") {
					netmask = "255.0.0.0";
				}
				else if (mask == "16") {
					netmask = "255.255.0.0";
				}
				else if (mask == "24") {
					netmask = "255.255.255.0";
				}
				else {
					LOGERROR("Error: Invalid netmask in %s", bridge_ip.c_str());

					close(m_hTAP);
					close(ip_fd);
					close(br_socket_fd);
					return false;
				}
			}

			LOGDEBUG("brctl addbr rascsi_bridge");

			if ((ret = ioctl(br_socket_fd, SIOCBRADDBR, "rascsi_bridge")) < 0) {
				LOGERROR("Error: can't ioctl SIOCBRADDBR. Errno: %d %s", errno, strerror(errno));

				close(m_hTAP);
				close(ip_fd);
				close(br_socket_fd);
				return false;
			}

			LOGDEBUG("ip address add %s dev rascsi_bridge", bridge_ip.c_str());

			struct ifreq ifr_a;
			ifr_a.ifr_addr.sa_family = AF_INET;
			strncpy(ifr_a.ifr_name, "rascsi_bridge", IFNAMSIZ);
			struct sockaddr_in* addr = (struct sockaddr_in*)&ifr_a.ifr_addr;
			inet_pton(AF_INET, address.c_str(), &addr->sin_addr);
			struct ifreq ifr_n;
			ifr_n.ifr_addr.sa_family = AF_INET;
			strncpy(ifr_n.ifr_name, "rascsi_bridge", IFNAMSIZ);
			struct sockaddr_in* mask = (struct sockaddr_in*)&ifr_n.ifr_addr;
			inet_pton(AF_INET, netmask.c_str(), &mask->sin_addr);
			if (ioctl(ip_fd, SIOCSIFADDR, &ifr_a) < 0 || ioctl(ip_fd, SIOCSIFNETMASK, &ifr_n) < 0) {
				LOGERROR("Error: can't ioctl SIOCSIFADDR or SIOCSIFNETMASK. Errno: %d %s", errno, strerror(errno));

				close(m_hTAP);
				close(ip_fd);
				close(br_socket_fd);
				return false;
			}
		}

		LOGDEBUG("ip link set dev rascsi_bridge up");

		if (!ip_link(ip_fd, "rascsi_bridge", TRUE)) {
			close(m_hTAP);
			close(ip_fd);
			close(br_socket_fd);
			return false;
		}
	}
	else
	{
		LOGINFO("rascsi_bridge is already available");
	}

	LOGDEBUG("ip link set ras0 up");

	if (!ip_link(ip_fd, "ras0", TRUE)) {
		close(m_hTAP);
		close(ip_fd);
		close(br_socket_fd);
		return false;
	}

	LOGDEBUG("brctl addif rascsi_bridge ras0");

	if (!br_setif(br_socket_fd, "rascsi_bridge", "ras0", TRUE)) {
		close(m_hTAP);
		close(ip_fd);
		close(br_socket_fd);
		return false;
	}

	// Get MAC address
	LOGTRACE("Getting the MAC address");

	ifr.ifr_addr.sa_family = AF_INET;
	if ((ret = ioctl(m_hTAP, SIOCGIFHWADDR, &ifr)) < 0) {
		LOGERROR("Error: can't ioctl SIOCGIFHWADDR. Errno: %d %s", errno, strerror(errno));

		close(m_hTAP);
		close(ip_fd);
		close(br_socket_fd);
		return false;
	}
	LOGTRACE("Got the MAC");

	// Save MAC address
	memcpy(m_MacAddr, ifr.ifr_hwaddr.sa_data, sizeof(m_MacAddr));

	close(ip_fd);
	close(br_socket_fd);

	LOGINFO("Tap device %s created", ifr.ifr_name);

	return true;
}
#endif // __linux__

#ifdef __NetBSD__
BOOL CTapDriver::Init()
{
	struct ifreq ifr;
	struct ifaddrs *ifa, *a;
	
	// TAP Device Initialization
	if ((m_hTAP = open("/dev/tap", O_RDWR)) < 0) {
		LOGERROR("Error: can't open tap. Errno: %d %s", errno, strerror(errno));
		return false;
	}

	// Get device name
	if (ioctl(m_hTAP, TAPGIFNAME, (void *)&ifr) < 0) {
		LOGERROR("Error: can't ioctl TAPGIFNAME. Errno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		return false;
	}

	// Get MAC address
	if (getifaddrs(&ifa) == -1) {
		LOGERROR("Error: can't getifaddrs. Errno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		return false;
	}
	for (a = ifa; a != NULL; a = a->ifa_next)
		if (strcmp(ifr.ifr_name, a->ifa_name) == 0 &&
			a->ifa_addr->sa_family == AF_LINK)
			break;
	if (a == NULL) {
		LOGERROR("Error: can't get MAC addressErrno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		return false;
	}

	// Save MAC address
	memcpy(m_MacAddr, LLADDR((struct sockaddr_dl *)a->ifa_addr),
		sizeof(m_MacAddr));
	freeifaddrs(ifa);

	LOGINFO("Tap device : %s\n", ifr.ifr_name);

	return true;
}
#endif // __NetBSD__

void CTapDriver::OpenDump(const Filepath& path) {
	if (m_pcap == NULL) {
		m_pcap = pcap_open_dead(DLT_EN10MB, 65535);
	}
	if (m_pcap_dumper != NULL) {
		pcap_dump_close(m_pcap_dumper);
	}
	m_pcap_dumper = pcap_dump_open(m_pcap, path.GetPath());
	if (m_pcap_dumper == NULL) {
		LOGERROR("Error: can't open pcap file: %s", pcap_geterr(m_pcap));
		throw io_exception("Can't open pcap file");
	}

	LOGTRACE("%s Opened %s for dumping", __PRETTY_FUNCTION__, path.GetPath());
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void CTapDriver::Cleanup()
{
	int br_socket_fd = -1;
	if ((br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
		LOGERROR("Error: can't open bridge socket. Errno: %d %s", errno, strerror(errno));
	} else {
		LOGDEBUG("brctl delif rascsi_bridge ras0");
		if (!br_setif(br_socket_fd, "rascsi_bridge", "ras0", FALSE)) {
			LOGWARN("Warning: Removing ras0 from the bridge failed.");
			LOGWARN("You may need to manually remove the ras0 tap device from the bridge");
		}
		close(br_socket_fd);
	}

	// Release TAP defice
	if (m_hTAP != -1) {
		close(m_hTAP);
		m_hTAP = -1;
	}

	if (m_pcap_dumper != NULL) {
		pcap_dump_close(m_pcap_dumper);
		m_pcap_dumper = NULL;
	}

	if (m_pcap != NULL) {
		pcap_close(m_pcap);
		m_pcap = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Enable
//
//---------------------------------------------------------------------------
bool CTapDriver::Enable(){
	int fd = socket(PF_INET, SOCK_DGRAM, 0);
	LOGDEBUG("%s: ip link set ras0 up", __PRETTY_FUNCTION__);
	bool result = ip_link(fd, "ras0", TRUE);
	close(fd);
	return result;
}

//---------------------------------------------------------------------------
//
//	Disable
//
//---------------------------------------------------------------------------
bool CTapDriver::Disable(){
	int fd = socket(PF_INET, SOCK_DGRAM, 0);
	LOGDEBUG("%s: ip link set ras0 down", __PRETTY_FUNCTION__);
	bool result = ip_link(fd, "ras0", FALSE);
	close(fd);
	return result;
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL CTapDriver::Flush(){
	LOGTRACE("%s", __PRETTY_FUNCTION__);
	while(PendingPackets()){
		(void)Rx(m_garbage_buffer);
	}
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	MGet MAC Address
//
//---------------------------------------------------------------------------
void CTapDriver::GetMacAddr(BYTE *mac)
{
	ASSERT(mac);

	memcpy(mac, m_MacAddr, sizeof(m_MacAddr));
}

//---------------------------------------------------------------------------
//
//	Receive
//
//---------------------------------------------------------------------------
BOOL CTapDriver::PendingPackets()
{
	struct pollfd fds;

	ASSERT(m_hTAP != -1);

	// Check if there is data that can be received
	fds.fd = m_hTAP;
	fds.events = POLLIN | POLLERR;
	fds.revents = 0;
	poll(&fds, 1, 0);
	LOGTRACE("%s %u revents", __PRETTY_FUNCTION__, fds.revents);
	if (!(fds.revents & POLLIN)) {
		return FALSE;
	}else {
		return TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Receive
//
//---------------------------------------------------------------------------
int CTapDriver::Rx(BYTE *buf)
{
	ASSERT(m_hTAP != -1);

	// Check if there is data that can be received
	if(!PendingPackets()){
		return 0;
	}

	// Receive
	DWORD dwReceived = read(m_hTAP, buf, ETH_FRAME_LEN);
	if (dwReceived == (DWORD)-1) {
		LOGWARN("%s Error occured while receiving an packet", __PRETTY_FUNCTION__);
		return 0;
	}

	// If reception is enabled
	if (dwReceived > 0) {
		// We need to add the Frame Check Status (FCS) CRC back onto the end of the packet.
		// The Linux network subsystem removes it, since most software apps shouldn't ever
		// need it.

		// Initialize the CRC
		DWORD crc = crc32(0L, Z_NULL, 0);
		// Calculate the CRC
		crc = crc32(crc, buf, dwReceived);

		buf[dwReceived + 0] = (BYTE)((crc >> 0) & 0xFF);
		buf[dwReceived + 1] = (BYTE)((crc >> 8) & 0xFF);
		buf[dwReceived + 2] = (BYTE)((crc >> 16) & 0xFF);
		buf[dwReceived + 3] = (BYTE)((crc >> 24) & 0xFF);

		LOGDEBUG("%s CRC is %08X - %02X %02X %02X %02X\n", __PRETTY_FUNCTION__, crc, buf[dwReceived+0], buf[dwReceived+1], buf[dwReceived+2], buf[dwReceived+3]);

		// Add FCS size to the received message size
		dwReceived += 4;
	}

	if (m_pcap_dumper != NULL) {
		struct pcap_pkthdr h = {
			.caplen = dwReceived,
			.len = dwReceived,
		};
		gettimeofday(&h.ts, NULL);
		pcap_dump((u_char*)m_pcap_dumper, &h, buf);
		LOGTRACE("%s Dumped %d byte packet (first byte: %02x last byte: %02x)", __PRETTY_FUNCTION__, (unsigned int)dwReceived, buf[0], buf[dwReceived-1]);
	}

	// Return the number of bytes
	return dwReceived;
}

//---------------------------------------------------------------------------
//
//	Send
//
//---------------------------------------------------------------------------
int CTapDriver::Tx(const BYTE *buf, int len)
{
	ASSERT(m_hTAP != -1);

	if (m_pcap_dumper != NULL) {
		struct pcap_pkthdr h = {
			.caplen = (bpf_u_int32)len,
			.len = (bpf_u_int32)len,
		};
		gettimeofday(&h.ts, NULL);
		pcap_dump((u_char*)m_pcap_dumper, &h, buf);
		LOGTRACE("%s Dumped %d byte packet (first byte: %02x last byte: %02x)", __PRETTY_FUNCTION__, (unsigned int)h.len, buf[0], buf[h.len-1]);
	}

	// Start sending
	return write(m_hTAP, buf, len);
}
