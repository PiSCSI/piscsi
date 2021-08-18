//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
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

#include "xm6.h"
#include "os.h"
#include "disk.h"
#include "ctapdriver.h"

//===========================================================================
//
//	DaynaPort SCSI Link
//
//===========================================================================
class SCSIDaynaPort: public Disk
{
public:
	// Basic Functions
	SCSIDaynaPort();
										// Constructor
	~SCSIDaynaPort();
										// Destructor
	void Open(const Filepath& path, BOOL attn = TRUE);
										// Capture packets

	// commands
	int Inquiry(const DWORD *cdb, BYTE *buffer) override;
										// INQUIRY command
	bool TestUnitReady(const DWORD *cdb) override;
										// TEST UNIT READY command
	int Read(const DWORD *cdb, BYTE *buf, DWORD block) override;
										// READ command
	bool Write(const DWORD *cdb, const BYTE *buf, DWORD block) override;
										// WRITE command
	int WriteCheck(DWORD block);
										// WRITE check

	int RetrieveStats(const DWORD *cdb, BYTE *buffer);
										// Retrieve DaynaPort statistics
	bool EnableInterface(const DWORD *cdb);
										// Enable/Disable Interface command

	void SetMacAddr(const DWORD *cdb, BYTE *buffer);
										// Set MAC address
	void SetMode(const DWORD *cdb, BYTE *buffer);
										// Set the mode: whether broadcast traffic is enabled or not
	static const BYTE CMD_SCSILINK_STATS        = 0x09;
	static const BYTE CMD_SCSILINK_ENABLE       = 0x0E;
	static const BYTE CMD_SCSILINK_SET          = 0x0C;
	static const BYTE CMD_SCSILINK_SETMODE      = 0x80;
	static const BYTE CMD_SCSILINK_SETMAC       = 0x40;

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
	typedef struct __attribute__((packed)) {
		BYTE operation_code;
		BYTE reserved;
		WORD pad;
		BYTE transfer_length;
		BYTE control;
	} scsi_cmd_config_multicast_t;

	typedef struct __attribute__((packed)) {
		BYTE operation_code;
		BYTE reserved;
		BYTE pad2;
		BYTE pad3;
		BYTE pad4;
		BYTE control;
	} scsi_cmd_enable_disable_iface_t;


	typedef struct __attribute__((packed)) {
		BYTE operation_code;
		BYTE misc_cdb_information;
		BYTE logical_block_address;
		WORD length;
		BYTE format;
	} scsi_cmd_daynaport_write_t;


	enum read_data_flags_t : DWORD {
		e_no_more_data = 0x00000000,
		e_more_data_available = 0x00000001,
		e_dropped_packets = 0xFFFFFFFF,
	};

	typedef struct __attribute__((packed)) {
		WORD length;
		read_data_flags_t flags;
		BYTE pad;
		BYTE data[ETH_FRAME_LEN + sizeof(DWORD)]; // Frame length + 4 byte CRC
	} scsi_resp_read_t;

	typedef struct __attribute__((packed)) {
		BYTE mac_address[6];
		DWORD frame_alignment_errors;
		DWORD crc_errors;
		DWORD frames_lost;
	} scsi_resp_link_stats_t;

	static const char* m_vendor_name;
	static const char* m_device_name;
	static const char* m_revision;
	static const char* m_firmware_version;

	scsi_resp_link_stats_t m_scsi_link_stats = {
		.mac_address = { 0x00, 0x80, 0x19, 0x10, 0x98, 0xE3 },//MAC address of @PotatoFi's DayanPort
		.frame_alignment_errors = 0,
		.crc_errors = 0,
		.frames_lost = 0,
	};

	const BYTE m_daynacom_mac_prefix[3] = {0x00,0x80,0x19};


	// Basic data
	// buf[0] ... Processor Device
	// buf[1] ... Not removable
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	//http://www.bitsavers.org/pdf/apple/scsi/dayna/daynaPORT/pocket_scsiLINK/pocketscsilink_inq.png
	const uint8_t m_daynaport_inquiry_response[44] =  {
		0x03, 0x00, 0x01, 0x00, // 4 bytes
		0x1F, 0x00, 0x00, 0x00, // 4 bytes
		// Vendor ID (8 Bytes)
		 'D','a','y','n','a',' ',' ',' ',
		// Product ID (16 Bytes)
		'S','C','S','I','/','L','i','n',
		'k',' ',' ',' ',' ',' ',' ',' ',
		// Revision Number (4 Bytes)
		'1','.','4','a',
		// Firmware Version (8 Bytes)
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
	};

	CTapDriver *m_tap;
										// TAP driver
	BOOL m_bTapEnable;
										// TAP valid flag
	BYTE m_mac_addr[6];
										// MAC Address
	static const BYTE m_bcast_addr[6];
	static const BYTE m_apple_talk_addr[6];
};
