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

#if defined(__linux__) && !defined(BAREMETAL)
	// TAP Driver Generation
	tap = new CTapDriver();
	m_bTapEnable = tap->Init();
	if(!m_bTapEnable){
		LOGERROR("Unable to open the TAP interface");
	}else {
		LOGINFO("Tap interface created")
	}

	// Generate MAC Address
	memset(mac_addr, 0x00, 6);
	if (m_bTapEnable) {
		tap->GetMacAddr(mac_addr);
		mac_addr[5]++;
	}
	// Packet reception flag OFF
	packet_enable = FALSE;
#endif	// RASCSI && !BAREMETAL
	LOGTRACE("SCSINuvolink Constructor End");

}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SCSINuvolink::~SCSINuvolink()
{
	LOGTRACE("SCSINuvolink Destructor");
#if !defined(BAREMETAL)
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
	const DWORD *cdb, BYTE *buffer, DWORD major, DWORD minor)
{
	DWORD response_size;
	DWORD temp_data_dword;
	WORD  temp_data_word;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buffer);
	ASSERT(cdb[0] == 0x12);

	LOGTRACE("%s Inquiry with major %ld, minor %ld",__PRETTY_FUNCTION__, major, minor);

	// The LSB of cdb[3] is used as an extension to the size
	// field in cdb[4]
	response_size = (((DWORD)cdb[3] & 0x1) << 8) + cdb[4];
	LOGWARN("size is %d (%08X)",(int)response_size, (WORD)response_size);

	for(int i=0; i< 5; i++)
	{
		LOGDEBUG("CDB Byte %d: %02X",i, (int)cdb[i]);
	}

	// EVPD check
	if (cdb[1] & 0x01) {
		LOGERROR("Invalid CDB = DISK_INVALIDCDB");
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Clear the buffer
	memset(buffer, 0, response_size);

	// Basic data
	// buffer[0] ... Communication Device
	// buffer[2] ... SCSI-2 compliant command system
	// buffer[3] ... SCSI-2 compliant Inquiry response
	// buffer[4] ... Inquiry additional data
	buffer[0] = 0x09; /* Communication device */

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buffer[0] = 0x7f;
	}
	/* SCSI 2 device */
	buffer[2] = 0x02;
	/* SCSI 2 response type */
	buffer[3] = (response_size >> 8) | 0x02;
	/* No additional length */
	buffer[4] = response_size;

	// Vendor name
	memcpy(&buffer[8], m_vendor_name, strlen( m_vendor_name));

	// Product name
	memcpy(&buffer[16], m_device_name, strlen(m_device_name));

	// Revision
	memcpy(&buffer[32], m_revision, strlen(m_revision));

	// MAC Address currently configured
	memcpy(&buffer[36], mac_addr, sizeof(mac_addr));

	// Build-in Hardware MAC address
	memcpy(&buffer[56], mac_addr, sizeof(mac_addr));

	// For now, all of the statistics are just garbage data to
	// make sure that the inquiry response is formatted correctly
	if(response_size > 96){
		// Header for SCSI statistics
		buffer[96] = 0x04; // Decimal 1234
		buffer[97] = 0xD2;

		// Header for SCSI errors section
		buffer[184]=0x09; // Decimal 2345
		buffer[185]=0x29;

		// Header for network statistics
		buffer[244]=0x0D;
		buffer[245]=0x80;

		// Received Packet Count
		temp_data_dword=100;
		memcpy(&buffer[246], &temp_data_dword, sizeof(temp_data_dword));

		// Transmitted Packet Count
		temp_data_dword=200;
		memcpy(&buffer[250], &temp_data_dword, sizeof(temp_data_dword));

		// Transmitted Request Count
		temp_data_dword=300;
		memcpy(&buffer[254], &temp_data_dword, sizeof(temp_data_dword));
		
		// Reset Count
		temp_data_word=50;
		memcpy(&buffer[258], &temp_data_word, sizeof(temp_data_word));

		// Header for network errors
		buffer[260]=0x11; // Decimal 4567
		buffer[261]=0xD7;

		// unexp_rst
		buffer[262]=0x01;
		buffer[263]=0x02;
		//transmit_errors
		buffer[264]=0x03;
		buffer[265]=0x04;
		// re_int
		buffer[266]=0x05;
		buffer[267]=0x06;
		// te_int
		buffer[268]=0x07;
		buffer[269]=0x08;
		// ow_int
		buffer[270]=0x09;
		buffer[271]=0x0A;
		buffer[272]=0x0B;
		buffer[273]=0x0C;
		buffer[274]=0x0D;
		buffer[275]=0x0E;
	 }

	//  Success
	disk.code = DISK_NOERROR;
	return response_size;
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
int FASTCALL SCSINuvolink::GetMessage6(const DWORD *cdb, BYTE *buffer)
{
	int type;
	int phase;
#if defined(RASCSI) && !defined(BAREMETAL)
	int func;
	int total_len;
	int i;
#endif	// RASCSI && !BAREMETAL

	ASSERT(this);
	LOGTRACE("SCSINuvolink::GetMessage");

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
					return GetMacAddr(buffer);

				case 1:		// Received packet acquisition (size/buffer)
					if (phase == 0) {
						// Get packet size
						ReceivePacket();
						buffer[0] = (BYTE)(packet_len >> 8);
						buffer[1] = (BYTE)packet_len;
						return 2;
					} else {
						// Get package data
						GetPacketBuf(buffer);
						return packet_len;
					}

				case 2:		// Received packet acquisition (size + buffer simultaneously)
					ReceivePacket();
					buffer[0] = (BYTE)(packet_len >> 8);
					buffer[1] = (BYTE)packet_len;
					GetPacketBuf(&buffer[2]);
					return packet_len + 2;

				case 3:		// Simultaneous acquisition of multiple packets (size + buffer simultaneously)
					// Currently the maximum number of packets is 10
					// Isn't it too fast if I increase more?
					total_len = 0;
					for (i = 0; i < 10; i++) {
						ReceivePacket();
						*buffer++ = (BYTE)(packet_len >> 8);
						*buffer++ = (BYTE)packet_len;
						total_len += 2;
						if (packet_len == 0)
							break;
						GetPacketBuf(buffer);
						buffer += packet_len;
						total_len += packet_len;
					}
					return total_len;
			}
			break;
#endif	// RASCSI && !BAREMETAL

		case 2:		// Host Drive
			// switch (phase) {
			// 	case 0:		// Get result code
			// 		return ReadFsResult(buffer);

			// 	case 1:		// Return data acquisition
			// 		return ReadFsOut(buffer);

			// 	case 2:		// Return additional data acquisition
			// 		return ReadFsOpt(buffer);
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
BOOL FASTCALL SCSINuvolink::SendMessage6(const DWORD *cdb, BYTE *buffer)
{
	// int type;
	int func;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buffer);
	LOGTRACE("%s Entering....", __PRETTY_FUNCTION__);

	// SendMessage6 is 6 bytes
	for(int i=0; i< 6; i++)
	{
		LOGDEBUG("%s Byte %d: %02X",__PRETTY_FUNCTION__, i, (int)cdb[i]);
	}

	// Type
	// type = cdb[2];

	// Function number
	func = cdb[0];

	// Phase
	// phase = cdb[9];

	// Get the number of bytes
	len = cdb[4];

	// // Do not process if TAP is invalid
	// if (!m_bTapEnable) {
	// 	LOGWARN("%s TAP is invalid and not working...", __PRETTY_FUNCTION__);
	// 	return FALSE;
	// }

	switch (func) {
		case 0x06:		// MAC address setting
			LOGWARN("%s Unhandled Set MAC Address happening here...", __PRETTY_FUNCTION__);
			SetMacAddr(buffer);
			return TRUE;
		case 0x09:		// Set Multicast Registers
			LOGWARN("%s Set Multicast registers happening here...", __PRETTY_FUNCTION__);
			SetMulticastRegisters(buffer, len);
			return TRUE;

		case 0x05:      // Send message
			LOGWARN("%s Send message happening here...(%d)", __PRETTY_FUNCTION__, len);
			// SendPacket(buffer,len);
			return TRUE;


		// case 1:		// Send packet
		// 	SendPacket(buffer, len);
		// 	return TRUE;
		default:
			LOGWARN("%s Unexpected command type found %02X",__PRETTY_FUNCTION__, func);
			break;
	}

	// Error
	ASSERT(FALSE);
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Set the Multicast Configuration
//
//---------------------------------------------------------------------------
void FASTCALL SCSINuvolink::SetMulticastRegisters(BYTE *buffer, int len){
	LOGINFO("%s I should be setting the multicast registers here, but I'll just print instead...", __PRETTY_FUNCTION__);
	LOGINFO("%s Got %d bytes", __PRETTY_FUNCTION__, len)
	for(int i=0; i<len; i++){
		LOGINFO("%s Byte %d: %02X", __PRETTY_FUNCTION__, i, (WORD)buffer[i]);
	}



}


#if !defined(BAREMETAL)
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
	LOGTRACE("%s Setting mac address", __PRETTY_FUNCTION__);

	for(size_t i=0; i<sizeof(mac_addr); i++)
	{
		LOGTRACE("%d: %02X",i,(WORD)mac[i]);
	}

	memcpy(mac_addr, mac, sizeof(mac_addr));
}

//---------------------------------------------------------------------------
//
//	Receive Packet
//
//---------------------------------------------------------------------------
int FASTCALL SCSINuvolink::ReceivePacket()
{
	static const BYTE bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	ASSERT(this);
	ASSERT(tap);
	//LOGTRACE("SCSINuvolink::ReceivePacket");

	// // previous packet has not been received
	// if (packet_enable) {
	// 	LOGDEBUG("packet not enabled");
	// 	return;
	// }

	// Receive packet
	packet_len = tap->Rx(packet_buf);

	if(packet_len){
		LOGINFO("Received a packet of size %d",packet_len);

		for(int i=0; i< 48; i+=8)
		{
			LOGTRACE("%s %02X: %02X %02X %02X %02X %02X %02X %02X %02X", \
				__PRETTY_FUNCTION__,i,
				(int)packet_buf[i+0],
				(int)packet_buf[i+1],
				(int)packet_buf[i+2],
				(int)packet_buf[i+3],
				(int)packet_buf[i+4],
				(int)packet_buf[i+5],
				(int)packet_buf[i+6],
				(int)packet_buf[i+7]);
		}
	}else{
		return -1;
	}

	// This is a very basic filter to prevent unnecessary packets from
	// being sent to the SCSI initiator. 

	// Check if received packet destination MAC address matches the
	// Nuvolink MAC
	if (memcmp(packet_buf, mac_addr, 6) != 0) {
		// If it doesn't match, check to see if this is a broadcast message
		if (memcmp(packet_buf, bcast_addr, 6) != 0) {
			LOGINFO("%s Received a packet that's not for me: %02X %02X %02X %02X %02X %02X", \
				__PRETTY_FUNCTION__,
				(int)packet_buf[0],
				(int)packet_buf[1],
				(int)packet_buf[2],
				(int)packet_buf[3],
				(int)packet_buf[4],
				(int)packet_buf[5]);
			packet_len = 0;
			return -1;
		}else{
			packet_is_bcast_or_mcast = TRUE;
		}
	}else{
		packet_is_bcast_or_mcast = FALSE;
	}

	// Discard if it exceeds the buffer size
	if (packet_len > 2048) {
		LOGDEBUG("Packet size was too big. Dropping it");
		packet_len = 0;
		return -1;
	}

	return packet_len;
}




//---------------------------------------------------------------------------
//
//	Get Packet
//
//---------------------------------------------------------------------------
void FASTCALL SCSINuvolink::GetPacketBuf(BYTE *buffer)
{
	int len;

	ASSERT(this);
	ASSERT(tap);
	ASSERT(buffer);
	LOGTRACE("SCSINuvolink::GetPacketBuf");

	// Size limit
	len = packet_len;
	if (len > 2048) {
		len = 2048;
	}

	// Copy
	memcpy(buffer, packet_buf, len);

	// Received
	packet_enable = FALSE;
}

//---------------------------------------------------------------------------
//
//	Send Packet
//
//---------------------------------------------------------------------------
BOOL SCSINuvolink::SendPacket(BYTE *buffer, int len)
{
	ASSERT(this);
	ASSERT(tap);
	ASSERT(buffer);

	LOGERROR("%s buffer addr %016lX len:%d", __PRETTY_FUNCTION__, (DWORD)buffer, len);

	LOGDEBUG("%s Total Len: %d", __PRETTY_FUNCTION__, len);
	LOGDEBUG("%s Destination Addr: %02X:%02X:%02X:%02X:%02X:%02X",\
		__PRETTY_FUNCTION__, (WORD)buffer[0], (WORD)buffer[1], (WORD)buffer[2], (WORD)buffer[3], (WORD)buffer[4], (WORD)buffer[5]);
	LOGDEBUG("%s Source Addr: %02X:%02X:%02X:%02X:%02X:%02X",\
		__PRETTY_FUNCTION__, (WORD)buffer[6], (WORD)buffer[7], (WORD)buffer[8], (WORD)buffer[9], (WORD)buffer[10], (WORD)buffer[11]);
	LOGDEBUG("%s EtherType: %02X:%02X", __PRETTY_FUNCTION__, (WORD)buffer[12], (WORD)buffer[13]);
	

	for(int i=0; i<len; i+=8){
	LOGDEBUG("Packet contents: %02X: %02X %02X %02X %02X %02X %02X %02X %02X", i,\
		(WORD)buffer[i],\
		(WORD)buffer[i+1],\
		(WORD)buffer[i+2],\
		(WORD)buffer[i+3],\
		(WORD)buffer[i+4],\
		(WORD)buffer[i+5],\
		(WORD)buffer[i+6],\
		(WORD)buffer[i+7]);
	}

	tap->Tx(buffer, len);
	return TRUE;
}
#endif	// RASCSI && !BAREMETAL

