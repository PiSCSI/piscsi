//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//  Copyright (C) 2020 akuker
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ Emulation of the DaynaPort SCSI Link Ethernet interface ]
//
//  This design is derived from the SLINKCMD.TXT file, as well as David Kuder's 
//  Tiny SCSI Emulator
//    - SLINKCMD: http://www.bitsavers.org/pdf/apple/scsi/dayna/daynaPORT/SLINKCMD.TXT
//    - Tiny SCSI : https://hackaday.io/project/18974-tiny-scsi-emulator
//
//  Special thanks to @PotatoFi for loaning me his Farallon EtherMac for
//  this development. (Farallon's EtherMac is a re-branded DaynaPort
//  SCSI/Link-T).
//
//  This does NOT include the file system functionality that is present
//  in the Sharp X68000 host bridge.
//
//  Note: This requires the DaynaPort SCSI Link driver.
//---------------------------------------------------------------------------
#pragma once

#include "os.h"
#include "disk.h"
#include "ctapdriver.h"
#include <string>
#include <array>

using namespace std;

//===========================================================================
//
//	DaynaPort SCSI Link
//
//===========================================================================
class SCSIDaynaPort: public Disk
{

public:
	SCSIDaynaPort();
	~SCSIDaynaPort() final;
	SCSIDaynaPort(SCSIDaynaPort&) = delete;
	SCSIDaynaPort& operator=(const SCSIDaynaPort&) = delete;

	bool Init(const unordered_map<string, string>&) override;
	void Open(const Filepath& path) override;

	// Commands
	vector<byte> InquiryInternal() const override;
	int Read(const vector<int>&, BYTE *, uint64_t) override;
	bool WriteBytes(const vector<int>&, const BYTE *, uint64_t);
	int WriteCheck(uint64_t block) override;

	int RetrieveStats(const vector<int>&, BYTE *) const;
	bool EnableInterface(const vector<int>&);

	void SetMacAddr(const vector<int>&, BYTE *);	// Set MAC address

	void TestUnitReady() override;
	void Read6() override;
	void Write6() override;
	void RetrieveStatistics();
	void SetInterfaceMode();
	void SetMcastAddr();
	void EnableInterface();
	int GetSendDelay() const override;

	bool Dispatch() override;

	const int DAYNAPORT_BUFFER_SIZE = 0x1000000;

	static const byte CMD_SCSILINK_STATS        = byte{0x09};
	static const byte CMD_SCSILINK_ENABLE       = byte{0x0E};
	static const byte CMD_SCSILINK_SET          = byte{0x0C};
	static const byte CMD_SCSILINK_SETMODE      = byte{0x80};
	static const byte CMD_SCSILINK_SETMAC       = byte{0x40};

	// When we're reading the Linux tap device, most of the messages will not be for us, so we
	// need to filter through those. However, we don't want to keep re-reading the packets
	// indefinitely. So, we'll pick a large-ish number that will cause the emulated DaynaPort
	// to respond with "no data" after MAX_READ_RETRIES tries.
	static const int MAX_READ_RETRIES               = 50;

	// The READ response has a header which consists of:
	//   2 bytes - payload size
	//   4 bytes - status flags
	static const DWORD DAYNAPORT_READ_HEADER_SZ = 2 + 4;

private:
	using super = Disk;

	Dispatcher<SCSIDaynaPort> dispatcher;

	enum class read_data_flags_t : uint32_t {
		e_no_more_data = 0x00000000,
		e_more_data_available = 0x00000001,
		e_dropped_packets = 0xFFFFFFFF,
	};

	using scsi_resp_read_t = struct __attribute__((packed)) {
		uint32_t length;
		read_data_flags_t flags;
		byte pad;
		byte data[ETH_FRAME_LEN + sizeof(uint32_t)]; // Frame length + 4 byte CRC
	};

	using scsi_resp_link_stats_t = struct __attribute__((packed)) {
		byte mac_address[6];
		uint32_t frame_alignment_errors;
		uint32_t crc_errors;
		uint32_t frames_lost;
	};

	scsi_resp_link_stats_t m_scsi_link_stats = {
		//MAC address of @PotatoFi's DayanPort
		.mac_address = { byte{0x00}, byte{0x80}, byte{0x19}, byte{0x10}, byte{0x98}, byte{0xE3} },
		.frame_alignment_errors = 0,
		.crc_errors = 0,
		.frames_lost = 0,
	};

	CTapDriver m_tap;

	// TAP valid flag
	bool m_bTapEnable = false;

	array<byte, 6> m_mac_addr;

	// static const array<byte, 6> m_bcast_addr;
	// static const array<byte, 6> m_apple_talk_addr;
};
