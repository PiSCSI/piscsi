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
#ifdef __linux__
#include <net/if.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#endif
#include <zlib.h> // For crc32()
#include "os.h"
#include "xm6.h"
#include "ctapdriver.h"
#include "log.h"
#include "exceptions.h"

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CTapDriver::CTapDriver()
{
	LOGTRACE("%s",__PRETTY_FUNCTION__);
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

BOOL CTapDriver::Init()
{
	LOGTRACE("%s",__PRETTY_FUNCTION__);

	char dev[IFNAMSIZ] = "ras0";
	struct ifreq ifr;
	int ret;

	LOGTRACE("Opening Tap device");
	// TAP device initilization
	if ((m_hTAP = open("/dev/net/tun", O_RDWR)) < 0) {
		LOGERROR("Error: can't open tun. Errno: %d %s", errno, strerror(errno));
		return FALSE;
	}
	LOGTRACE("Opened tap device %d",m_hTAP);

	
	// IFF_NO_PI for no extra packet information
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	LOGTRACE("Going to open %s", ifr.ifr_name);
	if ((ret = ioctl(m_hTAP, TUNSETIFF, (void *)&ifr)) < 0) {
		LOGERROR("Error: can't ioctl TUNSETIFF. Errno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		return FALSE;
	}
	LOGTRACE("return code from ioctl was %d", ret);

	int ip_fd;
	if ((ip_fd = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
		LOGERROR("Error: can't open ip socket. Errno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		return FALSE;
	}

	int br_socket_fd = -1;
	if ((br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0) {
		LOGERROR("Error: can't open bridge socket. Errno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		close(ip_fd);
		return FALSE;
	}

	LOGTRACE("Going to see if the bridge is created");

	// Check if the bridge is already created
	if (access("/sys/class/net/rascsi_bridge", F_OK) != 0) {
		LOGINFO("Creating the rascsi_bridge...");
		LOGDEBUG("brctl addbr rascsi_bridge");
		if ((ret = ioctl(br_socket_fd, SIOCBRADDBR, "rascsi_bridge")) < 0) {
			LOGERROR("Error: can't ioctl SIOCBRADDBR. Errno: %d %s", errno, strerror(errno));
			close(m_hTAP);
			close(ip_fd);
			close(br_socket_fd);
			return FALSE;
		}
		LOGDEBUG("brctl addif rascsi_bridge eth0");
		if (!br_setif(br_socket_fd, "rascsi_bridge", "eth0", TRUE)) {
			close(m_hTAP);
			close(ip_fd);
			close(br_socket_fd);
			return FALSE;
		}
		LOGDEBUG("ip link set dev rascsi_bridge up");
		if (!ip_link(ip_fd, "rascsi_bridge", TRUE)) {
			close(m_hTAP);
			close(ip_fd);
			close(br_socket_fd);
			return FALSE;
		}
	}
	else
	{
		LOGINFO("Note: rascsi_bridge already created");
	}

	LOGDEBUG("ip link set ras0 up");
	if (!ip_link(ip_fd, "ras0", TRUE)) {
		close(m_hTAP);
		close(ip_fd);
		close(br_socket_fd);
		return FALSE;
	}

	LOGDEBUG("brctl addif rascsi_bridge ras0");
	if (!br_setif(br_socket_fd, "rascsi_bridge", "ras0", TRUE)) {
		close(m_hTAP);
		close(ip_fd);
		close(br_socket_fd);
		return FALSE;
	}

	// Get MAC address
	LOGTRACE("Getting the MAC address");
	ifr.ifr_addr.sa_family = AF_INET;
	if ((ret = ioctl(m_hTAP, SIOCGIFHWADDR, &ifr)) < 0) {
		LOGERROR("Error: can't ioctl SIOCGIFHWADDR. Errno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		close(ip_fd);
		close(br_socket_fd);
		return FALSE;
	}
	LOGTRACE("got the mac");

	// Save MAC address
	memcpy(m_MacAddr, ifr.ifr_hwaddr.sa_data, sizeof(m_MacAddr));
	LOGINFO("Tap device %s created", ifr.ifr_name);

	close(ip_fd);
	close(br_socket_fd);

	return TRUE;
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
		return FALSE;
	}

	// Get device name
	if (ioctl(m_hTAP, TAPGIFNAME, (void *)&ifr) < 0) {
		LOGERROR("Error: can't ioctl TAPGIFNAME. Errno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		return FALSE;
	}

	// Get MAC address
	if (getifaddrs(&ifa) == -1) {
		LOGERROR("Error: can't getifaddrs. Errno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		return FALSE;
	}
	for (a = ifa; a != NULL; a = a->ifa_next)
		if (strcmp(ifr.ifr_name, a->ifa_name) == 0 &&
			a->ifa_addr->sa_family == AF_LINK)
			break;
	if (a == NULL) {
		LOGERROR("Error: can't get MAC addressErrno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		return FALSE;
	}

	// Save MAC address
	memcpy(m_MacAddr, LLADDR((struct sockaddr_dl *)a->ifa_addr),
		sizeof(m_MacAddr));
	freeifaddrs(ifa);

	LOGINFO("Tap device : %s\n", ifr.ifr_name);

	return TRUE;
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
