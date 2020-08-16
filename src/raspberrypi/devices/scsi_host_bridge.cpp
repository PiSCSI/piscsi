//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  Copyright (C) akuker
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ SCSI Host Bridge for the Sharp X68000 ]
//
//  Note: This requires a special driver on the host system and will only
//        work with the Sharp X68000 operating system.
//---------------------------------------------------------------------------

#include "scsi_host_bridge.h"
#include "xm6.h"
#include "ctapdriver.h"
#include "cfilesystem.h"

//===========================================================================
//
//	SCSI Host Bridge
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIBR::SCSIBR() : Disk()
{
	// Host Bridge
	disk.id = MAKEID('S', 'C', 'B', 'R');

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

	// Create host file system
	fs = new CFileSys();
	fs->Reset();
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SCSIBR::~SCSIBR()
{
#if defined(RASCSI) && !defined(BAREMETAL)
	// TAP driver release
	if (tap) {
		tap->Cleanup();
		delete tap;
	}
#endif	// RASCSI && !BAREMETAL

	// Release host file system
	if (fs) {
		fs->Reset();
		delete fs;
	}
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	char rev[32];
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Basic data
	// buf[0] ... Communication Device
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x09;

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}

	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 36 - 5 + 8;	// required + 8 byte extension

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Vendor name
	memcpy(&buf[8], BENDER_SIGNATURE, strlen(BENDER_SIGNATURE));

	// Product name
	memcpy(&buf[16], "RASCSI BRIDGE", 13);

	// Revision (XM6 version number)
	sprintf(rev, "0%01d%01d%01d",
				(int)major, (int)(minor >> 4), (int)(minor & 0x0f));
	memcpy(&buf[32], rev, 4);

	// Optional function valid flag
	buf[36] = '0';

#if defined(RASCSI) && !defined(BAREMETAL)
	// TAP Enable
	if (m_bTapEnable) {
		buf[37] = '1';
	}
#endif	// RASCSI && !BAREMETAL

	// CFileSys Enable
	buf[38] = '1';

	// Size of data that can be returned
	size = (buf[4] + 5);

	// Limit if the other buffer is small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	//  Success
	disk.code = DISK_NOERROR;
	return size;
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIBR::TestUnitReady(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// TEST UNIT READY Success
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	GET MESSAGE(10)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::GetMessage10(const DWORD *cdb, BYTE *buf)
{
	int type;
	int phase;
#if defined(RASCSI) && !defined(BAREMETAL)
	int func;
	int total_len;
	int i;
#endif	// RASCSI && !BAREMETAL

	ASSERT(this);

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
			switch (phase) {
				case 0:		// Get result code
					return ReadFsResult(buf);

				case 1:		// Return data acquisition
					return ReadFsOut(buf);

				case 2:		// Return additional data acquisition
					return ReadFsOpt(buf);
			}
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
BOOL FASTCALL SCSIBR::SendMessage10(const DWORD *cdb, BYTE *buf)
{
	int type;
	int func;
	int phase;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);

	// Type
	type = cdb[2];

	// Function number
	func = cdb[3];

	// Phase
	phase = cdb[9];

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
			switch (phase) {
				case 0:		// issue command
					WriteFs(func, buf);
					return TRUE;

				case 1:		// additional data writing
					WriteFsOpt(buf, len);
					return TRUE;
			}
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
int FASTCALL SCSIBR::GetMacAddr(BYTE *mac)
{
	ASSERT(this);
	ASSERT(mac);

	memcpy(mac, mac_addr, 6);
	return 6;
}

//---------------------------------------------------------------------------
//
//	Set MAC Address
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::SetMacAddr(BYTE *mac)
{
	ASSERT(this);
	ASSERT(mac);

	memcpy(mac_addr, mac, 6);
}

//---------------------------------------------------------------------------
//
//	Receive Packet
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::ReceivePacket()
{
	static const BYTE bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	ASSERT(this);
	ASSERT(tap);

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
void FASTCALL SCSIBR::GetPacketBuf(BYTE *buf)
{
	int len;

	ASSERT(this);
	ASSERT(tap);
	ASSERT(buf);

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
void FASTCALL SCSIBR::SendPacket(BYTE *buf, int len)
{
	ASSERT(this);
	ASSERT(tap);
	ASSERT(buf);

	tap->Tx(buf, len);
}
#endif	// RASCSI && !BAREMETAL

//---------------------------------------------------------------------------
//
//  $40 - Device Boot
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_InitDevice(BYTE *buf)
{
	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	fs->Reset();
	fsresult = fs->InitDevice((Human68k::argument_t*)buf);
}

//---------------------------------------------------------------------------
//
//  $41 - Directory Check
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_CheckDir(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->CheckDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $42 - Create Directory
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_MakeDir(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->MakeDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $43 - Remove Directory
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_RemoveDir(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->RemoveDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $44 - Rename
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Rename(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	Human68k::namests_t* pNamestsNew;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	pNamestsNew = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->Rename(nUnit, pNamests, pNamestsNew);
}

//---------------------------------------------------------------------------
//
//  $45 - Delete File
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Delete(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	fsresult = fs->Delete(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $46 - Get / Set file attributes
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Attribute(BYTE *buf)
{
	DWORD nUnit;
	Human68k::namests_t *pNamests;
	DWORD nHumanAttribute;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	dp = (DWORD*)&buf[i];
	nHumanAttribute = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->Attribute(nUnit, pNamests, nHumanAttribute);
}

//---------------------------------------------------------------------------
//
//  $47 - File Search
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Files(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::namests_t *pNamests;
	Human68k::files_t *files;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	files = (Human68k::files_t*)&buf[i];
	i += sizeof(Human68k::files_t);

	files->sector = ntohl(files->sector);
	files->offset = ntohs(files->offset);
	files->time = ntohs(files->time);
	files->date = ntohs(files->date);
	files->size = ntohl(files->size);

	fsresult = fs->Files(nUnit, nKey, pNamests, files);

	files->sector = htonl(files->sector);
	files->offset = htons(files->offset);
	files->time = htons(files->time);
	files->date = htons(files->date);
	files->size = htonl(files->size);

	i = 0;
	memcpy(&fsout[i], files, sizeof(Human68k::files_t));
	i += sizeof(Human68k::files_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $48 - File next search
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_NFiles(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::files_t *files;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	files = (Human68k::files_t*)&buf[i];
	i += sizeof(Human68k::files_t);

	files->sector = ntohl(files->sector);
	files->offset = ntohs(files->offset);
	files->time = ntohs(files->time);
	files->date = ntohs(files->date);
	files->size = ntohl(files->size);

	fsresult = fs->NFiles(nUnit, nKey, files);

	files->sector = htonl(files->sector);
	files->offset = htons(files->offset);
	files->time = htons(files->time);
	files->date = htons(files->date);
	files->size = htonl(files->size);

	i = 0;
	memcpy(&fsout[i], files, sizeof(Human68k::files_t));
	i += sizeof(Human68k::files_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $49 - File Creation
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Create(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::namests_t *pNamests;
	Human68k::fcb_t *pFcb;
	DWORD nAttribute;
	BOOL bForce;
	DWORD *dp;
	BOOL *bp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nAttribute = ntohl(*dp);
	i += sizeof(DWORD);

	bp = (BOOL*)&buf[i];
	bForce = ntohl(*bp);
	i += sizeof(BOOL);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Create(nUnit, nKey, pNamests, pFcb, nAttribute, bForce);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4A - Open File
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Open(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::namests_t *pNamests;
	Human68k::fcb_t *pFcb;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Open(nUnit, nKey, pNamests, pFcb);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4B - Close File
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Close(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Close(nUnit, nKey, pFcb);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4C - Read File
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Read(BYTE *buf)
{
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD nSize;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nSize = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Read(nKey, pFcb, fsopt, nSize);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;

	fsoptlen = fsresult;
}

//---------------------------------------------------------------------------
//
//  $4D - Write file
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Write(BYTE *buf)
{
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD nSize;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nSize = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Write(nKey, pFcb, fsopt, nSize);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4E - Seek file
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Seek(BYTE *buf)
{
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD nMode;
	int nOffset;
	DWORD *dp;
	int *ip;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nMode = ntohl(*dp);
	i += sizeof(DWORD);

	ip = (int*)&buf[i];
	nOffset = ntohl(*ip);
	i += sizeof(int);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->Seek(nKey, pFcb, nMode, nOffset);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $4F - File Timestamp Get / Set
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_TimeStamp(BYTE *buf)
{
	DWORD nUnit;
	DWORD nKey;
	Human68k::fcb_t *pFcb;
	DWORD nHumanTime;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nKey = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	nHumanTime = ntohl(*dp);
	i += sizeof(DWORD);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs->TimeStamp(nUnit, nKey, pFcb, nHumanTime);

	pFcb->fileptr = htonl(pFcb->fileptr);
	pFcb->mode = htons(pFcb->mode);
	pFcb->time = htons(pFcb->time);
	pFcb->date = htons(pFcb->date);
	pFcb->size = htonl(pFcb->size);

	i = 0;
	memcpy(&fsout[i], pFcb, sizeof(Human68k::fcb_t));
	i += sizeof(Human68k::fcb_t);

	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $50 - Get Capacity
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_GetCapacity(BYTE *buf)
{
	DWORD nUnit;
	Human68k::capacity_t cap;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->GetCapacity(nUnit, &cap);

	cap.freearea = htons(cap.freearea);
	cap.clusters = htons(cap.clusters);
	cap.sectors = htons(cap.sectors);
	cap.bytes = htons(cap.bytes);

	memcpy(fsout, &cap, sizeof(Human68k::capacity_t));
	fsoutlen = sizeof(Human68k::capacity_t);
}

//---------------------------------------------------------------------------
//
//  $51 - Drive status inspection/control
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_CtrlDrive(BYTE *buf)
{
	DWORD nUnit;
	Human68k::ctrldrive_t *pCtrlDrive;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	pCtrlDrive = (Human68k::ctrldrive_t*)&buf[i];
	i += sizeof(Human68k::ctrldrive_t);

	fsresult = fs->CtrlDrive(nUnit, pCtrlDrive);

	memcpy(fsout, pCtrlDrive, sizeof(Human68k::ctrldrive_t));
	fsoutlen = sizeof(Human68k::ctrldrive_t);
}

//---------------------------------------------------------------------------
//
//  $52 - Get DPB
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_GetDPB(BYTE *buf)
{
	DWORD nUnit;
	Human68k::dpb_t dpb;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->GetDPB(nUnit, &dpb);

	dpb.sector_size = htons(dpb.sector_size);
	dpb.fat_sector = htons(dpb.fat_sector);
	dpb.file_max = htons(dpb.file_max);
	dpb.data_sector = htons(dpb.data_sector);
	dpb.cluster_max = htons(dpb.cluster_max);
	dpb.root_sector = htons(dpb.root_sector);

	memcpy(fsout, &dpb, sizeof(Human68k::dpb_t));
	fsoutlen = sizeof(Human68k::dpb_t);
}

//---------------------------------------------------------------------------
//
//  $53 - Read Sector
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_DiskRead(BYTE *buf)
{
	DWORD nUnit;
	DWORD nSector;
	DWORD nSize;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nSector = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nSize = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->DiskRead(nUnit, fsout, nSector, nSize);
	fsoutlen = 0x200;
}

//---------------------------------------------------------------------------
//
//  $54 - Write Sector
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_DiskWrite(BYTE *buf)
{
	DWORD nUnit;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->DiskWrite(nUnit);
}

//---------------------------------------------------------------------------
//
//  $55 - IOCTRL
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Ioctrl(BYTE *buf)
{
	DWORD nUnit;
	DWORD nFunction;
	Human68k::ioctrl_t *pIoctrl;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	nFunction = ntohl(*dp);
	i += sizeof(DWORD);

	pIoctrl = (Human68k::ioctrl_t*)&buf[i];
	i += sizeof(Human68k::ioctrl_t);

	switch (nFunction) {
		case 2:
		case (DWORD)-2:
			pIoctrl->param = htonl(pIoctrl->param);
			break;
	}

	fsresult = fs->Ioctrl(nUnit, nFunction, pIoctrl);

	switch (nFunction) {
		case 0:
			pIoctrl->media = htons(pIoctrl->media);
			break;
		case 1:
		case (DWORD)-3:
			pIoctrl->param = htonl(pIoctrl->param);
			break;
	}

	i = 0;
	memcpy(&fsout[i], pIoctrl, sizeof(Human68k::ioctrl_t));
	i += sizeof(Human68k::ioctrl_t);
	fsoutlen = i;
}

//---------------------------------------------------------------------------
//
//  $56 - Flush
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Flush(BYTE *buf)
{
	DWORD nUnit;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->Flush(nUnit);
}

//---------------------------------------------------------------------------
//
//  $57 - Check Media
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_CheckMedia(BYTE *buf)
{
	DWORD nUnit;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->CheckMedia(nUnit);
}

//---------------------------------------------------------------------------
//
//  $58 - Lock
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::FS_Lock(BYTE *buf)
{
	DWORD nUnit;
	DWORD *dp;
	int i;

	ASSERT(this);
	ASSERT(fs);
	ASSERT(buf);

	i = 0;
	dp = (DWORD*)buf;
	nUnit = ntohl(*dp);
	i += sizeof(DWORD);

	fsresult = fs->Lock(nUnit);
}

//---------------------------------------------------------------------------
//
//	Read Filesystem (result code)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::ReadFsResult(BYTE *buf)
{
	DWORD *dp;

	ASSERT(this);
	ASSERT(buf);

	dp = (DWORD*)buf;
	*dp = htonl(fsresult);
	return sizeof(DWORD);
}

//---------------------------------------------------------------------------
//
//	Read Filesystem (return data)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::ReadFsOut(BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	memcpy(buf, fsout, fsoutlen);
	return fsoutlen;
}

//---------------------------------------------------------------------------
//
//	Read file system (return option data)
//
//---------------------------------------------------------------------------
int FASTCALL SCSIBR::ReadFsOpt(BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	memcpy(buf, fsopt, fsoptlen);
	return fsoptlen;
}

//---------------------------------------------------------------------------
//
//	Write Filesystem
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::WriteFs(int func, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	fsresult = FS_FATAL_INVALIDCOMMAND;
	fsoutlen = 0;
	fsoptlen = 0;

	// コマンド分岐
	func &= 0x1f;
	switch (func) {
		case 0x00: return FS_InitDevice(buf);	// $40 - start device
		case 0x01: return FS_CheckDir(buf);		// $41 - directory check
		case 0x02: return FS_MakeDir(buf);		// $42 - create directory
		case 0x03: return FS_RemoveDir(buf);	// $43 - remove directory
		case 0x04: return FS_Rename(buf);		// $44 - change file name
		case 0x05: return FS_Delete(buf);		// $45 - delete file
		case 0x06: return FS_Attribute(buf);	// $46 - Get/set file attribute
		case 0x07: return FS_Files(buf);		// $47 - file search
		case 0x08: return FS_NFiles(buf);		// $48 - next file search
		case 0x09: return FS_Create(buf);		// $49 - create file
		case 0x0A: return FS_Open(buf);			// $4A - File open
		case 0x0B: return FS_Close(buf);		// $4B - File close
		case 0x0C: return FS_Read(buf);			// $4C - read file
		case 0x0D: return FS_Write(buf);		// $4D - write file
		case 0x0E: return FS_Seek(buf);			// $4E - File seek
		case 0x0F: return FS_TimeStamp(buf);	// $4F - Get/set file modification time
		case 0x10: return FS_GetCapacity(buf);	// $50 - get capacity
		case 0x11: return FS_CtrlDrive(buf);	// $51 - Drive control/state check
		case 0x12: return FS_GetDPB(buf);		// $52 - Get DPB
		case 0x13: return FS_DiskRead(buf);		// $53 - read sector
		case 0x14: return FS_DiskWrite(buf);	// $54 - write sector
		case 0x15: return FS_Ioctrl(buf);		// $55 - IOCTRL
		case 0x16: return FS_Flush(buf);		// $56 - flush
		case 0x17: return FS_CheckMedia(buf);	// $57 - check media exchange
		case 0x18: return FS_Lock(buf);			// $58 - exclusive control
	}
}

//---------------------------------------------------------------------------
//
//	File system write (input option data)
//
//---------------------------------------------------------------------------
void FASTCALL SCSIBR::WriteFsOpt(BYTE *buf, int num)
{
	ASSERT(this);

	memcpy(fsopt, buf, num);
}
