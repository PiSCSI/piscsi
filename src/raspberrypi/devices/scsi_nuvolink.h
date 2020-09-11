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
//  [ Emulation of the Nuvolink SC SCSI Ethernet interface ]
//
//  This is based upon the fantastic documentation from Saybur on Gibhub
//  for the scuznet device. He did most of the hard work of reverse
//  engineering the protocol, documented here:
//     https://github.com/saybur/scuznet/blob/master/PROTOCOL.md
//
//  This does NOT include the file system functionality that is present
//  in the Sharp X68000 host bridge.
//
//  Note: This requires a the Nuvolink SC driver
//---------------------------------------------------------------------------
#pragma once

#include "xm6.h"
#include "os.h"
#include "disk.h"
#include "ctapdriver.h"

//===========================================================================
//
//	SCSI Nuvolink SC
//
//===========================================================================
class SCSINuvolink : public Disk
{
public:
	// Basic Functions
	SCSINuvolink();
										// Constructor
	virtual ~SCSINuvolink();
										// Destructor

	// commands
	int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
										// INQUIRY command
	BOOL FASTCALL TestUnitReady(const DWORD *cdb);
										// TEST UNIT READY command
	int FASTCALL GetMessage10(const DWORD *cdb, BYTE *buf);
										// GET MESSAGE10 command
	BOOL FASTCALL SendMessage10(const DWORD *cdb, BYTE *buf);
										// SEND MESSAGE10 command

private:
	enum nuvolink_command_enum : BYTE {
		cmmd_tstrdy = 0x00, // Test Unit Ready
		cmmd_ethrst = 0x02, // Reset Statistics (Vendor Specific)
		cmmd_rqsens = 0x03, // Request Sense
		cmmd_ethwrt = 0x05, // Send Package (Vendor Specific)
		cmmd_addr   = 0x06, // Change MAC address (Vendor Specific)
		cmmd_getmsg = 0x08, // Get Message
		cmmd_mcast  = 0x09, // Set Multicast Registers
		cmmd_sndmsg = 0x0A, // Send Message
		cmmd_mdsens = 0x0C, // Unknown Vendor Specific Command?
		cmmd_inq    = 0x12, // Inquiry
		cmmd_rdiag  = 0x1C, // Receive Diagnostic Results
		cmmd_sdiag  = 0x1D, // Send Diagnostic
	};

	static const char* m_vendor_name;
	static const char* m_device_name;
	static const char* m_revision;

#if defined(RASCSI) && !defined(BAREMETAL)
	int FASTCALL GetMacAddr(BYTE *buf);
										// Get MAC address
	void FASTCALL SetMacAddr(BYTE *buf);
										// Set MAC address
	void FASTCALL ReceivePacket();
										// Receive a packet
	void FASTCALL GetPacketBuf(BYTE *buf);
										// Get a packet
	void FASTCALL SendPacket(BYTE *buf, int len);
										// Send a packet

	CTapDriver *tap;
										// TAP driver
	BOOL m_bTapEnable;
										// TAP valid flag
	BYTE mac_addr[6];
										// MAC Addres
	int packet_len;
										// Receive packet size
	BYTE packet_buf[0x1000];
										// Receive packet buffer
	BOOL packet_enable;
										// Received packet valid
#endif	// RASCSI && !BAREMETAL

};
