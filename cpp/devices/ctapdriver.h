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

#include <pcap/pcap.h>
#include <net/ethernet.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <array>

using namespace std;

class CTapDriver
{
	static constexpr const char *BRIDGE_NAME = "piscsi_bridge";

public:

	CTapDriver() = default;
	~CTapDriver();
	CTapDriver(CTapDriver&) = default;
	CTapDriver& operator=(const CTapDriver&) = default;

	bool Init(const unordered_map<string, string>&);
	void OpenDump(const string& path);	// Capture packets
	void GetMacAddr(uint8_t *mac) const;
	int Receive(uint8_t *buf);
	int Send(const uint8_t *buf, int len);
	bool PendingPackets() const;		// Check if there are IP packets available
	bool Enable() const;		// Enable the piscsi0 interface
	bool Disable() const;		// Disable the piscsi0 interface
	void Flush();				// Purge all of the packets that are waiting to be processed

	static uint32_t Crc32(const uint8_t *, int);

private:
	array<byte, 6> m_MacAddr;	// MAC Address

	int m_hTAP = -1;			// File handle

	pcap_t *m_pcap = nullptr;
	pcap_dumper_t *m_pcap_dumper = nullptr;

	// Prioritized comma-separated list of interfaces to create the bridge for
	vector<string> interfaces;

	string inet;
};

