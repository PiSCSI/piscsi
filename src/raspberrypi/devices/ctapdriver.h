//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) akuker
//
//	[ TAP Driver ]
//
//---------------------------------------------------------------------------

#pragma once

#include <pcap/pcap.h>
#include <net/ethernet.h>
#include "filepath.h"
#include <unordered_map>
#include <list>
#include <string>
#include <array>

using namespace std; //NOSONAR Not relevant for rascsi

//===========================================================================
//
//	Linux Tap Driver
//
//===========================================================================
class CTapDriver
{
	friend class SCSIDaynaPort;
	friend class SCSIBR;

	static constexpr const char *BRIDGE_NAME = "rascsi_bridge";

	CTapDriver() = default;
	~CTapDriver();
	CTapDriver(CTapDriver&) = delete;
	CTapDriver& operator=(const CTapDriver&) = delete;

	bool Init(const unordered_map<string, string>&);

public:

	void OpenDump(const Filepath& path);	// Capture packets
	void GetMacAddr(BYTE *mac) const;	// Get Mac Address
	int Rx(BYTE *buf);					// Receive
	int Tx(const BYTE *buf, int len);	// Send
	bool PendingPackets() const;		// Check if there are IP packets available
	bool Enable() const;		// Enable the ras0 interface
	bool Disable() const;		// Disable the ras0 interface
	void Flush();				// Purge all of the packets that are waiting to be processed

private:
	array<byte, 6> m_MacAddr;	// MAC Address
	int m_hTAP = -1;			// File handle

	pcap_t *m_pcap = nullptr;
	pcap_dumper_t *m_pcap_dumper = nullptr;

	// Prioritized comma-separated list of interfaces to create the bridge for
	list<string> interfaces;

	string inet;
};

