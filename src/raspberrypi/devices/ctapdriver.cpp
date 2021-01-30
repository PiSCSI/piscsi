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

#include <zlib.h> // For crc32()
#include "os.h"
#include "xm6.h"
#include "ctapdriver.h"
#include "log.h"

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CTapDriver::CTapDriver()
{
	LOGTRACE("%s",__PRETTY_FUNCTION__);
	// Initialization
	m_hTAP = -1;
	memset(&m_MacAddr, 0, sizeof(m_MacAddr));
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
#ifdef __linux__
BOOL FASTCALL CTapDriver::Init()
{
	LOGTRACE("%s",__PRETTY_FUNCTION__);

	char dev[IFNAMSIZ] = "ras0";
	struct ifreq ifr;
	int ret;

	ASSERT(this);

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

	LOGDEBUG("ip link set ras0 up");
	ret = run_system_cmd("ip link set ras0 up");
	LOGTRACE("return code from ip link set ras0 up was %d", ret);
 
	LOGDEBUG("brctl addif rascsi_bridge ras0");
	ret = run_system_cmd("brctl addif rascsi_bridge ras0");
	LOGTRACE("return code from brctl addif rascsi_bridge ras0 was %d", ret);
	
	// Get MAC address
	LOGTRACE("Getting the MAC address");
	ifr.ifr_addr.sa_family = AF_INET;
	if ((ret = ioctl(m_hTAP, SIOCGIFHWADDR, &ifr)) < 0) {
		LOGERROR("Error: can't ioctl SIOCGIFHWADDR. Errno: %d %s", errno, strerror(errno));
		close(m_hTAP);
		return FALSE;
	}
	LOGTRACE("got the mac");

	// Save MAC address
	memcpy(m_MacAddr, ifr.ifr_hwaddr.sa_data, sizeof(m_MacAddr));
	LOGINFO("Tap device %s created", ifr.ifr_name);
	return TRUE;
}
#endif // __linux__

#ifdef __NetBSD__
BOOL FASTCALL CTapDriver::Init()
{
	struct ifreq ifr;
	struct ifaddrs *ifa, *a;
	
	ASSERT(this);

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

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CTapDriver::Cleanup()
{
	ASSERT(this);
	int result;

	LOGDEBUG("brctl delif rascsi_bridge ras0");
	result = run_system_cmd("brctl delif rascsi_bridge ras0");
	if(result != EXIT_SUCCESS){
		LOGWARN("Warning: The brctl delif command failed.");
		LOGWARN("You may need to manually remove the ras0 tap device from the bridge");
	}

	// Release TAP defice
	if (m_hTAP != -1) {
		close(m_hTAP);
		m_hTAP = -1;
	}
}

//---------------------------------------------------------------------------
//
//	Enable
//
//---------------------------------------------------------------------------
BOOL FASTCALL CTapDriver::Enable(){
	int result;
	LOGDEBUG("%s: ip link set ras0 up", __PRETTY_FUNCTION__);
	result = run_system_cmd("ip link set ras0 up");
	return (result == EXIT_SUCCESS);
}

//---------------------------------------------------------------------------
//
//	Disable
//
//---------------------------------------------------------------------------
BOOL FASTCALL CTapDriver::Disable(){
	int result;
	LOGDEBUG("%s: ip link set ras0 down", __PRETTY_FUNCTION__);
	result = run_system_cmd("ip link set ras0 down");
	return (result == EXIT_SUCCESS);
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL CTapDriver::Flush(){
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
void FASTCALL CTapDriver::GetMacAddr(BYTE *mac)
{
	ASSERT(this);
	ASSERT(mac);

	memcpy(mac, m_MacAddr, sizeof(m_MacAddr));
}

//---------------------------------------------------------------------------
//
//	Receive
//
//---------------------------------------------------------------------------
BOOL FASTCALL CTapDriver::PendingPackets()
{
	struct pollfd fds;

	ASSERT(this);
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
int FASTCALL CTapDriver::Rx(BYTE *buf)
{
	DWORD dwReceived;
	DWORD crc;

	ASSERT(this);
	ASSERT(m_hTAP != -1);

	// Check if there is data that can be received
	if(!PendingPackets()){
		return 0;
	}

	// Receive
	dwReceived = read(m_hTAP, buf, ETH_FRAME_LEN);
	if (dwReceived == (DWORD)-1) {
		return 0;
	}

	// If reception is enabled
	if (dwReceived > 0) {
		// We need to add the Frame Check Status (FCS) CRC back onto the end of the packet.
		// The Linux network subsystem removes it, since most software apps shouldn't ever
		// need it.

		// Initialize the CRC
		crc = crc32(0L, Z_NULL, 0);
		// Calculate the CRC
		crc = crc32(crc, buf, dwReceived);

		buf[dwReceived + 0] = (BYTE)((crc >> 0) & 0xFF);
		buf[dwReceived + 1] = (BYTE)((crc >> 8) & 0xFF);
		buf[dwReceived + 2] = (BYTE)((crc >> 16) & 0xFF);
		buf[dwReceived + 3] = (BYTE)((crc >> 24) & 0xFF);

		LOGDEBUG("%s CRC is %08lX - %02X %02X %02X %02X\n", __PRETTY_FUNCTION__, crc, buf[dwReceived+0], buf[dwReceived+1], buf[dwReceived+2], buf[dwReceived+3]);

		// Add FCS size to the received message size
		dwReceived += 4;
	}

	// Return the number of bytes
	return dwReceived;
}

//---------------------------------------------------------------------------
//
//	Send
//
//---------------------------------------------------------------------------
int FASTCALL CTapDriver::Tx(const BYTE *buf, int len)
{
	ASSERT(this);
	ASSERT(m_hTAP != -1);

	// Start sending
	return write(m_hTAP, buf, len);
}
