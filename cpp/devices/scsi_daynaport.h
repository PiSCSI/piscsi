//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//  Copyright (C) 2020 akuker
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//  Copyright (C) 2023 Uwe Seimet
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
//  Note: This requires a DaynaPort SCSI Link driver.
//---------------------------------------------------------------------------

#pragma once

#include "primary_device.h"
#include "ctapdriver.h"
#include <net/ethernet.h>
#include <string>
#include <span>
#include <unordered_map>
#include <array>

//===========================================================================
//
//	DaynaPort SCSI Link
//
//===========================================================================
class SCSIDaynaPort : public PrimaryDevice
{
	uint64_t byte_read_count = 0;
	uint64_t byte_write_count = 0;

	inline static const string BYTE_READ_COUNT = "byte_read_count";
	inline static const string BYTE_WRITE_COUNT = "byte_write_count";

public:

	explicit SCSIDaynaPort(int);
	~SCSIDaynaPort() override = default;

	bool Init(const param_map&) override;
	void CleanUp() override;

	param_map GetDefaultParams() const override { return tap.GetDefaultParams(); }

	// Commands
	vector<uint8_t> InquiryInternal() const override;
	int Read(cdb_t, vector<uint8_t>&, uint64_t);
	bool Write(cdb_t, span<const uint8_t>);

	int RetrieveStats(cdb_t, vector<uint8_t>&) const;

	void TestUnitReady() override;
	void Read6();
	void Write6() const;
	void RetrieveStatistics() const;
	void SetInterfaceMode() const;
	void SetMcastAddr() const;
	void EnableInterface() const;

	vector<PbStatistics> GetStatistics() const override;

	static const int DAYNAPORT_BUFFER_SIZE = 0x1000000;

	static const int CMD_SCSILINK_STATS        = 0x09;
	static const int CMD_SCSILINK_ENABLE       = 0x0E;
	static const int CMD_SCSILINK_SET          = 0x0C;
	static const int CMD_SCSILINK_SETMAC       = 0x40;
	static const int CMD_SCSILINK_SETMODE      = 0x80;

	// When we're reading the Linux tap device, most of the messages will not be for us, so we
	// need to filter through those. However, we don't want to keep re-reading the packets
	// indefinitely. So, we'll pick a large-ish number that will cause the emulated DaynaPort
	// to respond with "no data" after MAX_READ_RETRIES tries.
	static const int MAX_READ_RETRIES               = 50;

	// The READ response has a header which consists of:
	//   2 bytes - payload size
	//   4 bytes - status flags
	static const uint32_t DAYNAPORT_READ_HEADER_SZ = 2 + 4;

private:

	enum class read_data_flags_t : uint32_t {
		e_no_more_data = 0x00000000,
		e_more_data_available = 0x00000001,
		e_dropped_packets = 0xFFFFFFFF,
	};

	using scsi_resp_read_t = struct __attribute__((packed)) {
		uint32_t length;
		read_data_flags_t flags;
		byte pad;
		array<byte, ETH_FRAME_LEN + sizeof(uint32_t)> data; // Frame length + 4 byte CRC
	};

	using scsi_resp_link_stats_t = struct __attribute__((packed)) {
		array<byte, 6> mac_address;
		uint32_t frame_alignment_errors;
		uint32_t crc_errors;
		uint32_t frames_lost;
	};

	scsi_resp_link_stats_t m_scsi_link_stats = {
		// TODO Remove this hard-coded MAC address, see https://github.com/PiSCSI/piscsi/issues/598
		.mac_address = { byte{0x00}, byte{0x80}, byte{0x19}, byte{0x10}, byte{0x98}, byte{0xe3} },
		.frame_alignment_errors = 0,
		.crc_errors = 0,
		.frames_lost = 0,
	};

	CTapDriver tap;

	bool tap_enabled = false;
};
