//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) akuker
//
//---------------------------------------------------------------------------

#pragma once

#include "devices/device.h"
#include "shared/network_util.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <array>
#include <span>

#ifndef ETH_FRAME_LEN
static const int ETH_FRAME_LEN = 1514;
#endif
#ifndef ETH_FCS_LEN
static const int ETH_FCS_LEN = 4;
#endif

using namespace std;

class CTapDriver
{
	static const string BRIDGE_NAME;

	const inline static string DEFAULT_MODE = "bridge";

public:

	CTapDriver() = default;
	~CTapDriver() = default;
	CTapDriver(CTapDriver&) = default;
	CTapDriver& operator=(const CTapDriver&) = default;

	bool Init(const param_map&);
	void CleanUp();

	param_map GetDefaultParams() const;
	static string GetProfileValidationError(const string&, const string&, const network_util::network_interface_map&);

	void GetMacAddr(uint8_t *) const;
	int Receive(uint8_t *) const;
	int Send(const uint8_t *, int) const;
	bool HasPendingPackets() const;		// Check if there are IP packets available
	string IpLink(bool) const;	// Enable/Disable the piscsi0 interface
	void Flush() const;			// Purge all of the packets that are waiting to be processed

	static uint32_t Crc32(span<const uint8_t>);
	static array<uint8_t, 6> DeriveDaynaPortMac(span<const uint8_t>);

private:

	bool CreateTap();
	string SetTapUp() const;
	string AttachTapToBridge();
	void ReleaseTap();

	// The kernel TAP port and emulated DaynaPort must use different MACs. A
	// bridge treats a port's MAC as local and will not forward frames for it.
	array<uint8_t, 6> m_MacAddr {};
#ifdef __linux__
	array<uint8_t, 6> m_TapMac {};
#endif

	int m_hTAP = -1;			// File handle
	bool tap_attached_to_bridge = false;
};
