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
#include "filepath.h"
#include <unordered_map>
#include <list>
#include <string>

#ifndef ETH_FRAME_LEN
#define ETH_FRAME_LEN 1514
#endif

using namespace std;

//===========================================================================
//
//	Linux Tap Driver
//
//===========================================================================
class CTapDriver
{
private:

	friend class SCSIDaynaPort;
	friend class SCSIBR;

	CTapDriver() = default;
	~CTapDriver() = default;
	CTapDriver(CTapDriver&) = delete;
	CTapDriver& operator=(const CTapDriver&) = delete;

	bool Init(const unordered_map<string, string>&);

public:

	void OpenDump(const Filepath& path);
										// Capture packets
	void Cleanup();						// Cleanup
	void GetMacAddr(BYTE *mac) const;	// Get Mac Address
	int Rx(BYTE *buf);					// Receive
	int Tx(const BYTE *buf, int len);	// Send
	bool PendingPackets() const;		// Check if there are IP packets available
	bool Enable() const;		// Enable the ras0 interface
	bool Disable() const;		// Disable the ras0 interface
	void Flush();				// Purge all of the packets that are waiting to be processed

private:
	BYTE m_MacAddr[6] = {};						// MAC Address
	int m_hTAP = -1;							// File handle

	BYTE m_garbage_buffer[ETH_FRAME_LEN];

	pcap_t *m_pcap = nullptr;
	pcap_dumper_t *m_pcap_dumper = nullptr;

	// Prioritized comma-separated list of interfaces to create the bridge for
	list<string> interfaces;

	string inet;
};

