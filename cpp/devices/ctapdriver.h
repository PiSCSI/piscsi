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

public:

	CTapDriver() = default;
	~CTapDriver() = default;
	CTapDriver(CTapDriver&) = default;
	CTapDriver& operator=(const CTapDriver&) = default;

	bool Init(const unordered_map<string, string>&);
	void CleanUp() const;

	void GetMacAddr(uint8_t *) const;
	int Receive(uint8_t *) const;
	int Send(const uint8_t *, int) const;
	bool HasPendingPackets() const;		// Check if there are IP packets available
	string IpLink(bool) const;	// Enable/Disable the piscsi0 interface
	void Flush() const;			// Purge all of the packets that are waiting to be processed

	static uint32_t Crc32(span<const uint8_t>);

private:

	static string SetUpEth0(int, const string&);
	static string SetUpNonEth0(int, int, const string&);
	static pair<string, string> ExtractAddressAndMask(const string&);

	array<byte, 6> m_MacAddr;	// MAC Address

	int m_hTAP = -1;			// File handle

	// Prioritized comma-separated list of interfaces to create the bridge for
	vector<string> interfaces;

	string inet;
};

