//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License. 
//	See LICENSE file in the project root folder.
//
//	[ SCSI Host Bridge for the Sharp X68000 ]
//
//  Note: This requires a special driver on the host system and will only
//        work with the Sharp X68000 operating system.
//---------------------------------------------------------------------------

#include "rascsi_exceptions.h"
#include "scsi_command_util.h"
#include "dispatcher.h"
#include "scsi_host_bridge.h"
#include <arpa/inet.h>
#include <array>

using namespace std;
using namespace scsi_defs;
using namespace scsi_command_util;

SCSIBR::SCSIBR() : Disk("SCBR")
{
	// Create host file system
	fs.Reset();

	dispatcher.Add(scsi_command::eCmdTestUnitReady, "TestUnitReady", &SCSIBR::TestUnitReady);
	dispatcher.Add(scsi_command::eCmdRead6, "GetMessage10", &SCSIBR::GetMessage10);
	dispatcher.Add(scsi_command::eCmdWrite6, "SendMessage10", &SCSIBR::SendMessage10);
}

SCSIBR::~SCSIBR()
{
	// Release host file system
	fs.Reset();
}

bool SCSIBR::Init(const unordered_map<string, string>& params)
{
	SetParams(params);

#ifdef __linux
	// TAP Driver Generation
	m_bTapEnable = tap.Init(GetParams());
	if (!m_bTapEnable){
		LOGERROR("Unable to open the TAP interface")
		return false;
	}

	// Generate MAC Address
	memset(mac_addr, 0x00, 6);
	if (m_bTapEnable) {
		tap.GetMacAddr(mac_addr);
		mac_addr[5]++;
	}

	// Packet reception flag OFF
	packet_enable = false;
#endif

	SetReady(m_bTapEnable);

// Not terminating on regular Linux PCs is helpful for testing
#if defined(__x86_64__) || defined(__X86__)
	return true;
#else
	return m_bTapEnable;
#endif
}

bool SCSIBR::Dispatch(scsi_command cmd)
{
	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, cmd) ? true : super::Dispatch(cmd);
}

vector<byte> SCSIBR::InquiryInternal() const
{
	vector<byte> b = HandleInquiry(device_type::COMMUNICATIONS, scsi_level::SCSI_2, false);

	// The bridge returns 6 more additional bytes than the other devices
	auto buf = vector<byte>(0x1F + 8 + 5);
	memcpy(buf.data(), b.data(), b.size());

	buf[4] = byte{0x1F + 8};

	// Optional function valid flag
	buf[36] = byte{'0'};

	// TAP Enable
	if (m_bTapEnable) {
		buf[37] = byte{'1'};
	}

	// CFileSys Enable
	buf[38] = byte{'1'};

	return buf;
}

void SCSIBR::TestUnitReady()
{
	// Always successful
	EnterStatusPhase();
}

int SCSIBR::GetMessage10(const vector<int>& cdb, BYTE *buf)
{
	// Type
	int type = cdb[2];

	// Function number
	int func = cdb[3];

	// Phase
	int phase = cdb[9];

	switch (type) {
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
						SetInt16(&buf[0], packet_len);
						return 2;
					} else {
						// Get package data
						GetPacketBuf(buf);
						return packet_len;
					}

				case 2:		// Received packet acquisition (size + buffer simultaneously)
					ReceivePacket();
					SetInt16(&buf[0], packet_len);
					GetPacketBuf(&buf[2]);
					return packet_len + 2;

				case 3:	{
					// Simultaneous acquisition of multiple packets (size + buffer simultaneously)
					// Currently the maximum number of packets is 10
					// Isn't it too fast if I increase more?
					int total_len = 0;
					for (int i = 0; i < 10; i++) {
						ReceivePacket();
						SetInt16(&buf[0], packet_len);
						// TODO Callee should not modify buffer pointer
						buf += 2;
						total_len += 2;
						if (packet_len == 0)
							break;
						GetPacketBuf(buf);
						buf += packet_len;
						total_len += packet_len;
					}
					return total_len;
				}

				default:
					assert(false);
					return -1;
			}
			break;

		case 2:		// Host Drive
			switch (phase) {
				case 0:		// Get result code
					return ReadFsResult(buf);

				case 1:		// Return data acquisition
					return ReadFsOut(buf);

				case 2:		// Return additional data acquisition
					return ReadFsOpt(buf);

				default:
					break;
			}
			break;

			default:
				break;
	}

	// Error
	assert(false);
	return 0;
}

bool SCSIBR::WriteBytes(const vector<int>& cdb, BYTE *buf, uint64_t)
{
	// Type
	int type = cdb[2];

	// Function number
	int func = cdb[3];

	// Phase
	int phase = cdb[9];

	// Get the number of lights
	int len = cdb[6];
	len <<= 8;
	len |= cdb[7];
	len <<= 8;
	len |= cdb[8];

	switch (type) {
		case 1:		// Ethernet
			// Do not process if TAP is invalid
			if (!m_bTapEnable) {
				return false;
			}

			switch (func) {
				case 0:		// MAC address setting
					SetMacAddr(buf);
					return true;

				case 1:		// Send packet
					SendPacket(buf, len);
					return true;

				default:
					break;
			}
			break;

		case 2:		// Host drive
			switch (phase) {
				case 0:		// issue command
					WriteFs(func, buf);
					return true;

				case 1:		// additional data writing
					WriteFsOpt(buf, len);
					return true;

				default:
					break;
			}
			break;

			default:
		break;
	}

	assert(false);
	return false;
}

void SCSIBR::GetMessage10()
{
	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl->bufsize < 0x1000000) {
		delete[] ctrl->buffer;
		ctrl->bufsize = 0x1000000;
		ctrl->buffer = new BYTE[ctrl->bufsize];
	}

	ctrl->length = GetMessage10(ctrl->cmd, ctrl->buffer);
	if (ctrl->length <= 0) {
		throw scsi_error_exception();
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	EnterDataInPhase();
}

//---------------------------------------------------------------------------
//
//	SEND MESSAGE(10)
//
//  This Send Message command is used by the X68000 host driver
//
//---------------------------------------------------------------------------
void SCSIBR::SendMessage10()
{
	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl->bufsize < 0x1000000) {
		delete[] ctrl->buffer;
		ctrl->bufsize = 0x1000000;
		ctrl->buffer = new BYTE[ctrl->bufsize];
	}

	// Set transfer amount
	ctrl->length = ctrl->cmd[6];
	ctrl->length <<= 8;
	ctrl->length |= ctrl->cmd[7];
	ctrl->length <<= 8;
	ctrl->length |= ctrl->cmd[8];

	if (ctrl->length <= 0) {
		throw scsi_error_exception();
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	EnterDataOutPhase();
}

int SCSIBR::GetMacAddr(BYTE *mac) const
{
	memcpy(mac, mac_addr, 6);
	return 6;
}

void SCSIBR::SetMacAddr(const BYTE *mac)
{
	memcpy(mac_addr, mac, 6);
}

void SCSIBR::ReceivePacket()
{
	static const array<BYTE, 6> bcast_addr = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	// previous packet has not been received
	if (packet_enable) {
		return;
	}

	// Receive packet
	packet_len = tap.Rx(packet_buf);

	// Check if received packet
	if (memcmp(packet_buf, mac_addr, 6) != 0 && memcmp(packet_buf, bcast_addr.data(), bcast_addr.size()) != 0) {
		packet_len = 0;
		return;
	}

	// Discard if it exceeds the buffer size
	if (packet_len > 2048) {
		packet_len = 0;
		return;
	}

	// Store in receive buffer
	if (packet_len > 0) {
		packet_enable = true;
	}
}

void SCSIBR::GetPacketBuf(BYTE *buf)
{
	// Size limit
	int len = packet_len;
	if (len > 2048) {
		len = 2048;
	}

	// Copy
	memcpy(buf, packet_buf, len);

	// Received
	packet_enable = false;
}

void SCSIBR::SendPacket(const BYTE *buf, int len)
{
	tap.Tx(buf, len);
}

//---------------------------------------------------------------------------
//
//  $40 - Device Boot
//
//---------------------------------------------------------------------------
void SCSIBR::FS_InitDevice(BYTE *buf)
{
	fs.Reset();
	fsresult = fs.InitDevice((Human68k::argument_t*)buf);
}

//---------------------------------------------------------------------------
//
//  $41 - Directory Check
//
//---------------------------------------------------------------------------
void SCSIBR::FS_CheckDir(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	const auto pNamests = (Human68k::namests_t*)&buf[i];

	fsresult = fs.CheckDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $42 - Create Directory
//
//---------------------------------------------------------------------------
void SCSIBR::FS_MakeDir(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	const auto pNamests = (Human68k::namests_t*)&buf[i];

	fsresult = fs.MakeDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $43 - Remove Directory
//
//---------------------------------------------------------------------------
void SCSIBR::FS_RemoveDir(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	const auto pNamests = (Human68k::namests_t*)&buf[i];

	fsresult = fs.RemoveDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $44 - Rename
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Rename(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	const auto pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	const Human68k::namests_t *pNamestsNew = (Human68k::namests_t*)&buf[i];

	fsresult = fs.Rename(nUnit, pNamests, pNamestsNew);
}

//---------------------------------------------------------------------------
//
//  $45 - Delete File
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Delete(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	const auto *pNamests = (Human68k::namests_t*)&buf[i];

	fsresult = fs.Delete(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $46 - Get / Set file attributes
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Attribute(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	const auto pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	dp = (DWORD*)&buf[i];
	DWORD nHumanAttribute = ntohl(*dp);

	fsresult = fs.Attribute(nUnit, pNamests, nHumanAttribute);
}

//---------------------------------------------------------------------------
//
//  $47 - File Search
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Files(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	DWORD nKey = ntohl(*dp);
	i += sizeof(DWORD);

	const auto pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	auto files = (Human68k::files_t*)&buf[i];

	files->sector = ntohl(files->sector);
	files->offset = ntohs(files->offset);
	files->time = ntohs(files->time);
	files->date = ntohs(files->date);
	files->size = ntohl(files->size);

	fsresult = fs.Files(nUnit, nKey, pNamests, files);

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
void SCSIBR::FS_NFiles(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	DWORD nKey = ntohl(*dp);
	i += sizeof(DWORD);

	auto files = (Human68k::files_t*)&buf[i];

	files->sector = ntohl(files->sector);
	files->offset = ntohs(files->offset);
	files->time = ntohs(files->time);
	files->date = ntohs(files->date);
	files->size = ntohl(files->size);

	fsresult = fs.NFiles(nUnit, nKey, files);

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
void SCSIBR::FS_Create(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	DWORD nKey = ntohl(*dp);
	i += sizeof(DWORD);

	const auto pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	auto pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	DWORD nAttribute = ntohl(*dp);
	i += sizeof(DWORD);

	auto bp = (int*)&buf[i];
	DWORD bForce = ntohl(*bp);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs.Create(nUnit, nKey, pNamests, pFcb, nAttribute, bForce);

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
void SCSIBR::FS_Open(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	DWORD nKey = ntohl(*dp);
	i += sizeof(DWORD);

	const auto pNamests = (Human68k::namests_t*)&buf[i];
	i += sizeof(Human68k::namests_t);

	auto pFcb = (Human68k::fcb_t*)&buf[i];

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs.Open(nUnit, nKey, pNamests, pFcb);

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
void SCSIBR::FS_Close(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	DWORD nKey = ntohl(*dp);
	i += sizeof(DWORD);

	auto pFcb = (Human68k::fcb_t*)&buf[i];

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs.Close(nUnit, nKey, pFcb);

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
void SCSIBR::FS_Read(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nKey = ntohl(*dp);
	int i = sizeof(DWORD);

	auto pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	DWORD nSize = ntohl(*dp);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs.Read(nKey, pFcb, fsopt, nSize);

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
void SCSIBR::FS_Write(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nKey = ntohl(*dp);
	int i = sizeof(DWORD);

	auto pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	DWORD nSize = ntohl(*dp);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs.Write(nKey, pFcb, fsopt, nSize);

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
void SCSIBR::FS_Seek(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nKey = ntohl(*dp);
	int i = sizeof(DWORD);

	auto pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	DWORD nMode = ntohl(*dp);
	i += sizeof(DWORD);

	auto ip = (const int*)&buf[i];
	int nOffset = ntohl(*ip);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs.Seek(nKey, pFcb, nMode, nOffset);

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
void SCSIBR::FS_TimeStamp(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	DWORD nKey = ntohl(*dp);
	i += sizeof(DWORD);

	auto pFcb = (Human68k::fcb_t*)&buf[i];
	i += sizeof(Human68k::fcb_t);

	dp = (DWORD*)&buf[i];
	DWORD nHumanTime = ntohl(*dp);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs.TimeStamp(nUnit, nKey, pFcb, nHumanTime);

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
void SCSIBR::FS_GetCapacity(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);

	Human68k::capacity_t cap;
	fsresult = fs.GetCapacity(nUnit, &cap);

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
void SCSIBR::FS_CtrlDrive(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	auto pCtrlDrive = (Human68k::ctrldrive_t*)&buf[i];

	fsresult = fs.CtrlDrive(nUnit, pCtrlDrive);

	memcpy(fsout, pCtrlDrive, sizeof(Human68k::ctrldrive_t));
	fsoutlen = sizeof(Human68k::ctrldrive_t);
}

//---------------------------------------------------------------------------
//
//  $52 - Get DPB
//
//---------------------------------------------------------------------------
void SCSIBR::FS_GetDPB(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);

	Human68k::dpb_t dpb;
	fsresult = fs.GetDPB(nUnit, &dpb);

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
void SCSIBR::FS_DiskRead(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	DWORD nSector = ntohl(*dp);
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	DWORD nSize = ntohl(*dp);

	fsresult = fs.DiskRead(nUnit, fsout, nSector, nSize);
	fsoutlen = 0x200;
}

//---------------------------------------------------------------------------
//
//  $54 - Write Sector
//
//---------------------------------------------------------------------------
void SCSIBR::FS_DiskWrite(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);

	fsresult = fs.DiskWrite(nUnit);
}

//---------------------------------------------------------------------------
//
//  $55 - IOCTRL
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Ioctrl(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);
	int i = sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	DWORD nFunction = ntohl(*dp);
	i += sizeof(DWORD);

	auto pIoctrl = (Human68k::ioctrl_t*)&buf[i];

	switch (nFunction) {
		case 2:
		case (DWORD)-2:
			pIoctrl->param = htonl(pIoctrl->param);
			break;
		default:
			break;
	}

	fsresult = fs.Ioctrl(nUnit, nFunction, pIoctrl);

	switch (nFunction) {
		case 0:
			pIoctrl->media = htons(pIoctrl->media);
			break;
		case 1:
		case (DWORD)-3:
			pIoctrl->param = htonl(pIoctrl->param);
			break;
		default:
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
void SCSIBR::FS_Flush(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);

	fsresult = fs.Flush(nUnit);
}

//---------------------------------------------------------------------------
//
//  $57 - Check Media
//
//---------------------------------------------------------------------------
void SCSIBR::FS_CheckMedia(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);

	fsresult = fs.CheckMedia(nUnit);
}

//---------------------------------------------------------------------------
//
//  $58 - Lock
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Lock(BYTE *buf)
{
	auto dp = (DWORD*)buf;
	DWORD nUnit = ntohl(*dp);

	fsresult = fs.Lock(nUnit);
}

//---------------------------------------------------------------------------
//
//	Read Filesystem (result code)
//
//---------------------------------------------------------------------------
int SCSIBR::ReadFsResult(BYTE *buf) const
{
	auto dp = (DWORD*)buf;
	*dp = htonl(fsresult);
	return sizeof(DWORD);
}

//---------------------------------------------------------------------------
//
//	Read Filesystem (return data)
//
//---------------------------------------------------------------------------
int SCSIBR::ReadFsOut(BYTE *buf) const
{
	memcpy(buf, fsout, fsoutlen);
	return fsoutlen;
}

//---------------------------------------------------------------------------
//
//	Read file system (return option data)
//
//---------------------------------------------------------------------------
int SCSIBR::ReadFsOpt(BYTE *buf) const
{
	memcpy(buf, fsopt, fsoptlen);
	return fsoptlen;
}

//---------------------------------------------------------------------------
//
//	Write Filesystem
//
//---------------------------------------------------------------------------
void SCSIBR::WriteFs(int func, BYTE *buf)
{
	fsresult = FS_FATAL_INVALIDCOMMAND;
	fsoutlen = 0;
	fsoptlen = 0;

	func &= 0x1f;
	switch (func) {
		case 0x00: return FS_InitDevice(buf);	// $40 - start device
		case 0x01: return FS_CheckDir(buf);	// $41 - directory check
		case 0x02: return FS_MakeDir(buf);	// $42 - create directory
		case 0x03: return FS_RemoveDir(buf);	// $43 - remove directory
		case 0x04: return FS_Rename(buf);	// $44 - change file name
		case 0x05: return FS_Delete(buf);	// $45 - delete file
		case 0x06: return FS_Attribute(buf);	// $46 - Get/set file attribute
		case 0x07: return FS_Files(buf);	// $47 - file search
		case 0x08: return FS_NFiles(buf);	// $48 - next file search
		case 0x09: return FS_Create(buf);	// $49 - create file
		case 0x0A: return FS_Open(buf);		// $4A - File open
		case 0x0B: return FS_Close(buf);	// $4B - File close
		case 0x0C: return FS_Read(buf);		// $4C - read file
		case 0x0D: return FS_Write(buf);	// $4D - write file
		case 0x0E: return FS_Seek(buf);		// $4E - File seek
		case 0x0F: return FS_TimeStamp(buf);	// $4F - Get/set file modification time
		case 0x10: return FS_GetCapacity(buf);	// $50 - get capacity
		case 0x11: return FS_CtrlDrive(buf);	// $51 - Drive control/state check
		case 0x12: return FS_GetDPB(buf);	// $52 - Get DPB
		case 0x13: return FS_DiskRead(buf);	// $53 - read sector
		case 0x14: return FS_DiskWrite(buf);	// $54 - write sector
		case 0x15: return FS_Ioctrl(buf);	// $55 - IOCTRL
		case 0x16: return FS_Flush(buf);	// $56 - flush
		case 0x17: return FS_CheckMedia(buf);	// $57 - check media exchange
		case 0x18: return FS_Lock(buf);		// $58 - exclusive control
		default: break;
	}
}

//---------------------------------------------------------------------------
//
//	File system write (input option data)
//
//---------------------------------------------------------------------------
void SCSIBR::WriteFsOpt(const BYTE *buf, int num)
{
	memcpy(fsopt, buf, num);
}
