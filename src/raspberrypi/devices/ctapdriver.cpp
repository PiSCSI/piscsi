//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
//
//	[ TAP Driver ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "ctapdriver.h"

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CTapDriver::CTapDriver()
{
	// 初期化
	m_hTAP = -1;
	memset(&m_MacAddr, 0, sizeof(m_MacAddr));
}

//---------------------------------------------------------------------------
//
//	初期化
//
//---------------------------------------------------------------------------
#ifdef __linux__
BOOL FASTCALL CTapDriver::Init()
{
	char dev[IFNAMSIZ] = "ras0";
	struct ifreq ifr;
	int ret;

	ASSERT(this);

	// TAPデバイス初期化
	if ((m_hTAP = open("/dev/net/tun", O_RDWR)) < 0) {
		printf("Error: can't open tun\n");
		return FALSE;
	}

	// IFF_NO_PI for no extra packet information
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
	strncpy(ifr.ifr_name, dev, IFNAMSIZ);
	if ((ret = ioctl(m_hTAP, TUNSETIFF, (void *)&ifr)) < 0) {
		printf("Error: can't ioctl TUNSETIFF\n");
		close(m_hTAP);
		return FALSE;
	}

	// MACアドレス取得
	ifr.ifr_addr.sa_family = AF_INET;
	if ((ret = ioctl(m_hTAP, SIOCGIFHWADDR, &ifr)) < 0) {
		printf("Error: can't ioctl SIOCGIFHWADDR\n");
		close(m_hTAP);
		return FALSE;
	}

	// MACアドレス保存
	memcpy(m_MacAddr, ifr.ifr_hwaddr.sa_data, sizeof(m_MacAddr));
	return TRUE;
}
#endif // __linux__

#ifdef __NetBSD__
BOOL FASTCALL CTapDriver::Init()
{
	struct ifreq ifr;
	struct ifaddrs *ifa, *a;
	
	ASSERT(this);

	// TAPデバイス初期化
	if ((m_hTAP = open("/dev/tap", O_RDWR)) < 0) {
		printf("Error: can't open tap\n");
		return FALSE;
	}

	// デバイス名取得
	if (ioctl(m_hTAP, TAPGIFNAME, (void *)&ifr) < 0) {
		printf("Error: can't ioctl TAPGIFNAME\n");
		close(m_hTAP);
		return FALSE;
	}

	// MACアドレス取得
	if (getifaddrs(&ifa) == -1) {
		printf("Error: can't getifaddrs\n");
		close(m_hTAP);
		return FALSE;
	}
	for (a = ifa; a != NULL; a = a->ifa_next)
		if (strcmp(ifr.ifr_name, a->ifa_name) == 0 &&
			a->ifa_addr->sa_family == AF_LINK)
			break;
	if (a == NULL) {
		printf("Error: can't get MAC address\n");
		close(m_hTAP);
		return FALSE;
	}

	// MACアドレス保存
	memcpy(m_MacAddr, LLADDR((struct sockaddr_dl *)a->ifa_addr),
		sizeof(m_MacAddr));
	freeifaddrs(ifa);

	printf("Tap device : %s\n", ifr.ifr_name);

	return TRUE;
}
#endif // __NetBSD__

//---------------------------------------------------------------------------
//
//	クリーンアップ
//
//---------------------------------------------------------------------------
void FASTCALL CTapDriver::Cleanup()
{
	ASSERT(this);

	// TAPデバイス解放
	if (m_hTAP != -1) {
		close(m_hTAP);
		m_hTAP = -1;
	}
}

//---------------------------------------------------------------------------
//
//	MACアドレス取得
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
//	受信
//
//---------------------------------------------------------------------------
int FASTCALL CTapDriver::Rx(BYTE *buf)
{
	struct pollfd fds;
	DWORD dwReceived;

	ASSERT(this);
	ASSERT(m_hTAP != -1);

	// 受信可能なデータがあるか調べる
	fds.fd = m_hTAP;
	fds.events = POLLIN | POLLERR;
	fds.revents = 0;
	poll(&fds, 1, 0);
	if (!(fds.revents & POLLIN)) {
		return 0;
	}

	// 受信
	dwReceived = read(m_hTAP, buf, ETH_FRAME_LEN);
	if (dwReceived == (DWORD)-1) {
		return 0;
	}

	// 受信が有効であれば
	if (dwReceived > 0) {
		// FCSを除く最小フレームサイズ(60バイト)にパディング
		if (dwReceived < 60) {
			memset(buf + dwReceived, 0, 60 - dwReceived);
			dwReceived = 60;
		}

		// ダミーのFCSを付加する
		memset(buf + dwReceived, 0, 4);
		dwReceived += 4;
	}

	// バイト数を返却
	return dwReceived;
}

//---------------------------------------------------------------------------
//
//	送信
//
//---------------------------------------------------------------------------
int FASTCALL CTapDriver::Tx(BYTE *buf, int len)
{
	ASSERT(this);
	ASSERT(m_hTAP != -1);

	// 送信開始
	return write(m_hTAP, buf, len);
}
