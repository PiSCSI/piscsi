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

#include "scsi_nuvolink.h"

//===========================================================================
//
//	SCSI Nuvolink SC Ethernet Adapter
//
//===========================================================================
const char* SCSINuvolink::m_vendor_name = "Nuvotech";
const char* SCSINuvolink::m_device_name = "NuvoSC";
const char* SCSINuvolink::m_revision = "1.1r";


//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSINuvolink::SCSINuvolink() : Disk()
{
	LOGTRACE("SCSINuvolink Constructor");
	// Nuvolink
	disk.id = MAKEID('S', 'C', 'N', 'L');

#if defined(RASCSI) && defined(__linux__) && !defined(BAREMETAL)
	// TAP Driver Generation
	tap = new CTapDriver();
	m_bTapEnable = tap->Init();

	// Generate MAC Address
	memset(mac_addr, 0x00, 6);
	if (m_bTapEnable) {
		tap->GetMacAddr(mac_addr);
		mac_addr[5]++;
	}

	// Packet reception flag OFF
	packet_enable = FALSE;
#endif	// RASCSI && !BAREMETAL
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SCSINuvolink::~SCSINuvolink()
{
	LOGTRACE("SCSINuvolink Destructor");
#if defined(RASCSI) && !defined(BAREMETAL)
	// TAP driver release
	if (tap) {
		tap->Cleanup();
		delete tap;
	}
#endif	// RASCSI && !BAREMETAL
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSINuvolink::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	char rev[32];
	int size;
	WORD extended_size=0;
	DWORD junk_data=0;
	WORD junk_data_word=0;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	LOGTRACE("SCSINuvolink::Inquiry");
	LOGINFO("Inquiry with major %ld, minor %ld",major, minor);

	// The INQUIRY command from the Nuvolink can be 16 bits long
	extended_size = (WORD)cdb[4] + (((WORD)cdb[3])<<8);
	LOGINFO("Inquiry Size was %d", extended_size);

	for(int i=0; i< 5; i++)
	{
		LOGINFO("CDB Byte %d: %02X",i,cdb[i]);
	}

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	/* The protocol documentaiton says that the size should never be
	 * greater than 292 */
	if(extended_size > 292)
	{
		LOGWARN("Extended size was greater than 292. Limiting to 292 instead of %d", extended_size);
		extended_size = 292;
	}

	// Clear the buffer
	memset(buf, 0, extended_size);


	// Basic data
	// buf[0] ... Communication Device
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	buf[0] = 0x09; /* Communication device */

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}
	/* SCSI 2 device */
	buf[2] = 0x02;
	/* SCSI 2 response type */
	buf[3] = 0x02;
	/* No additional length */
	buf[4] = 0;

	// Vendor name
	memcpy(&buf[8], m_vendor_name, strlen( m_vendor_name));

	// Product name
	memcpy(&buf[16], m_device_name, strlen(m_device_name));

	// Revision
	memcpy(&buf[32], m_revision, strlen(m_revision));

	// MAC Address currently configured
	memcpy(&buf[36], mac_addr, sizeof(mac_addr));

	// Build-in Hardware MAC address
	memcpy(&buf[36], mac_addr, sizeof(mac_addr));

	// If this is an extended inquiry, add some additional details
	if(extended_size >= 292)
	{
		// Header for SCSI statistics
		buf[96] = 0x04; // Decimal 1234
		buf[97] = 0xD2;

		// Header for SCSI errors section
		buf[184]=0x09; // Decimal 2345
		buf[185]=0x29;

		// Header for network statistics
		buf[244]=0x0D;
		buf[245]=0x80;

		// Received Packet Count
		junk_data=100;
		memcpy(&buf[246], &junk_data, sizeof(junk_data));

		// Transmitted Packet Count
		junk_data=200;
		memcpy(&buf[250], &junk_data, sizeof(junk_data));
	
		// Transmitted Request Count
		junk_data=300;
		memcpy(&buf[254], &junk_data, sizeof(junk_data));
		
		// Reset Count
		junk_data_word=50;
		memcpy(&buf[258], &junk_data_word, sizeof(junk_data_word));

		// Header for network errors
		buf[260]=0x11; // Decimal 4567
		buf[261]=0xD7;
	}

	//  Success
	disk.code = DISK_NOERROR;
	return extended_size;
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSINuvolink::TestUnitReady(const DWORD* /*cdb*/)
{
	ASSERT(this);
	LOGTRACE("SCSINuvolink::TestUnitReady");

	// TEST UNIT READY Success
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	GET MESSAGE(10)
//
//---------------------------------------------------------------------------
int FASTCALL SCSINuvolink::GetMessage10(const DWORD *cdb, BYTE *buf)
{
	int type;
	int phase;
#if defined(RASCSI) && !defined(BAREMETAL)
	int func;
	int total_len;
	int i;
#endif	// RASCSI && !BAREMETAL

	ASSERT(this);
	LOGTRACE("SCSINuvolink::GetMessage10");

	// Type
	type = cdb[2];

#if defined(RASCSI) && !defined(BAREMETAL)
	// Function number
	func = cdb[3];
#endif	// RASCSI && !BAREMETAL

	// Phase
	phase = cdb[9];

	switch (type) {
#if defined(RASCSI) && !defined(BAREMETAL)
		case 1:		// Ethernet
			// Do not process if TAP is invalid
			if (!m_bTapEnable) {
				return 0;
			}

			switch (func) {
				case 0:		// Get MAC address
					return GetMacAddr(buf);

				case 1:		// Received packet acquisition (size/buffer)
					if (phase == 0) {
						// Get packet size
						ReceivePacket();
						buf[0] = (BYTE)(packet_len >> 8);
						buf[1] = (BYTE)packet_len;
						return 2;
					} else {
						// Get package data
						GetPacketBuf(buf);
						return packet_len;
					}

				case 2:		// Received packet acquisition (size + buffer simultaneously)
					ReceivePacket();
					buf[0] = (BYTE)(packet_len >> 8);
					buf[1] = (BYTE)packet_len;
					GetPacketBuf(&buf[2]);
					return packet_len + 2;

				case 3:		// Simultaneous acquisition of multiple packets (size + buffer simultaneously)
					// Currently the maximum number of packets is 10
					// Isn't it too fast if I increase more?
					total_len = 0;
					for (i = 0; i < 10; i++) {
						ReceivePacket();
						*buf++ = (BYTE)(packet_len >> 8);
						*buf++ = (BYTE)packet_len;
						total_len += 2;
						if (packet_len == 0)
							break;
						GetPacketBuf(buf);
						buf += packet_len;
						total_len += packet_len;
					}
					return total_len;
			}
			break;
#endif	// RASCSI && !BAREMETAL

		case 2:		// Host Drive
			// switch (phase) {
			// 	case 0:		// Get result code
			// 		return ReadFsResult(buf);

			// 	case 1:		// Return data acquisition
			// 		return ReadFsOut(buf);

			// 	case 2:		// Return additional data acquisition
			// 		return ReadFsOpt(buf);
			// }
			break;
	}

	// Error
	ASSERT(FALSE);
	return 0;
}

//---------------------------------------------------------------------------
//
//	SEND MESSAGE(10)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSINuvolink::SendMessage10(const DWORD *cdb, BYTE *buf)
{
	int type;
	int func;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	LOGTRACE("SCSINuvolink::SendMessage10");

	// Type
	type = cdb[2];

	// Function number
	func = cdb[3];

	// Phase
	// phase = cdb[9];

	// Get the number of lights
	len = cdb[6];
	len <<= 8;
	len |= cdb[7];
	len <<= 8;
	len |= cdb[8];

	switch (type) {
#if defined(RASCSI) && !defined(BAREMETAL)
		case 1:		// Ethernet
			// Do not process if TAP is invalid
			if (!m_bTapEnable) {
				return FALSE;
			}

			switch (func) {
				case 0:		// MAC address setting
					SetMacAddr(buf);
					return TRUE;

				case 1:		// Send packet
					SendPacket(buf, len);
					return TRUE;
			}
			break;
#endif	// RASCSI && !BAREMETAL

		case 2:		// Host drive
			// switch (phase) {
			// 	case 0:		// issue command
			// 		WriteFs(func, buf);
			// 		return TRUE;

			// 	case 1:		// additional data writing
			// 		WriteFsOpt(buf, len);
			// 		return TRUE;
			// }
			break;
	}

	// Error
	ASSERT(FALSE);
	return FALSE;
}

#if defined(RASCSI) && !defined(BAREMETAL)
//---------------------------------------------------------------------------
//
//	Get MAC Address
//
//---------------------------------------------------------------------------
int FASTCALL SCSINuvolink::GetMacAddr(BYTE *mac)
{
	ASSERT(this);
	ASSERT(mac);
	LOGTRACE("SCSINuvolink::GetMacAddr");

	memcpy(mac, mac_addr, 6);
	return 6;
}

//---------------------------------------------------------------------------
//
//	Set MAC Address
//
//---------------------------------------------------------------------------
void FASTCALL SCSINuvolink::SetMacAddr(BYTE *mac)
{
	ASSERT(this);
	ASSERT(mac);
	LOGTRACE("SCSINuvolink::SetMacAddr");

	memcpy(mac_addr, mac, 6);
}

//---------------------------------------------------------------------------
//
//	Receive Packet
//
//---------------------------------------------------------------------------
void FASTCALL SCSINuvolink::ReceivePacket()
{
	static const BYTE bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	ASSERT(this);
	ASSERT(tap);
	LOGTRACE("SCSINuvolink::ReceivePacket");

	// previous packet has not been received
	if (packet_enable) {
		return;
	}

	// Receive packet
	packet_len = tap->Rx(packet_buf);

	// Check if received packet
	if (memcmp(packet_buf, mac_addr, 6) != 0) {
		if (memcmp(packet_buf, bcast_addr, 6) != 0) {
			packet_len = 0;
			return;
		}
	}

	// Discard if it exceeds the buffer size
	if (packet_len > 2048) {
		packet_len = 0;
		return;
	}

	// Store in receive buffer
	if (packet_len > 0) {
		packet_enable = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Get Packet
//
//---------------------------------------------------------------------------
void FASTCALL SCSINuvolink::GetPacketBuf(BYTE *buf)
{
	int len;

	ASSERT(this);
	ASSERT(tap);
	ASSERT(buf);
	LOGTRACE("SCSINuvolink::GetPacketBuf");

	// Size limit
	len = packet_len;
	if (len > 2048) {
		len = 2048;
	}

	// Copy
	memcpy(buf, packet_buf, len);

	// Received
	packet_enable = FALSE;
}

//---------------------------------------------------------------------------
//
//	Send Packet
//
//---------------------------------------------------------------------------
void FASTCALL SCSINuvolink::SendPacket(BYTE *buf, int len)
{
	ASSERT(this);
	ASSERT(tap);
	ASSERT(buf);
	LOGTRACE("SCSINuvolink::SendPacket");

	tap->Tx(buf, len);
}
#endif	// RASCSI && !BAREMETAL

