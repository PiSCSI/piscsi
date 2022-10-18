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

SCSIBR::SCSIBR(int lun) : Disk(SCBR, lun)
{
	// Create host file system
	fs.Reset();

	dispatcher.Add(scsi_command::eCmdTestUnitReady, "TestUnitReady", &SCSIBR::TestUnitReady);
	dispatcher.Add(scsi_command::eCmdRead6, "GetMessage10", &SCSIBR::GetMessage10);
	dispatcher.Add(scsi_command::eCmdWrite6, "SendMessage10", &SCSIBR::SendMessage10);
}

bool SCSIBR::Init(const unordered_map<string, string>& params)
{
	SetParams(params);

#ifdef __linux__
	// TAP Driver Generation
	m_bTapEnable = tap.Init(GetParams());
	if (!m_bTapEnable){
		LOGERROR("Unable to open the TAP interface")
		return false;
	}
#endif

	// Generate MAC Address
	if (m_bTapEnable) {
		tap.GetMacAddr(mac_addr.data());
		mac_addr[5]++;
	}

	// Packet reception flag OFF
	packet_enable = false;

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
	vector<byte> buf = HandleInquiry(device_type::COMMUNICATIONS, scsi_level::SCSI_2, false);

	// The bridge returns more additional bytes than the other devices
	buf.resize(0x1F + 8 + 5);

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

int SCSIBR::GetMessage10(const vector<int>& cdb, vector<BYTE>& buf)
{
	// Type
	const int type = cdb[2];

	// Function number
	const int func = cdb[3];

	// Phase
	const int phase = cdb[9];

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
						SetInt16(buf, 0, packet_len);
						return 2;
					} else {
						// Get package data
						GetPacketBuf(buf, 0);
						return packet_len;
					}

				case 2:		// Received packet acquisition (size + buffer simultaneously)
					ReceivePacket();
					SetInt16(buf, 0, packet_len);
					GetPacketBuf(buf, 2);
					return packet_len + 2;

				case 3:	{
					// Simultaneous acquisition of multiple packets (size + buffer simultaneously)
					// Currently the maximum number of packets is 10
					// Isn't it too fast if I increase more?
					int total_len = 0;
					for (int i = 0; i < 10; i++) {
						ReceivePacket();
						SetInt16(buf, total_len, packet_len);
						total_len += 2;
						if (packet_len == 0)
							break;
						GetPacketBuf(buf, 0);
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

bool SCSIBR::WriteBytes(const vector<int>& cdb, vector<BYTE>& buf, uint64_t)
{
	// Type
	const int type = cdb[2];

	// Function number
	const int func = cdb[3];

	// Phase
	const int phase = cdb[9];

	// Get the number of lights
	const int len = GetInt24(cdb, 6);

	switch (type) {
		case 1:		// Ethernet
			// Do not process if TAP is invalid
			if (!m_bTapEnable) {
				return false;
			}

			switch (func) {
				case 0:
					SetMacAddr(buf);
					return true;

				case 1:
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
	// Ensure a sufficient buffer size (because it is not a transfer for each block)
	controller->AllocateBuffer(0x1000000);

	ctrl->length = GetMessage10(ctrl->cmd, controller->GetBuffer());
	if (ctrl->length <= 0) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
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
	ctrl->length = GetInt24(ctrl->cmd, 6);
	if (ctrl->length <= 0) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Ensure a sufficient buffer size (because it is not a transfer for each block)
	controller->AllocateBuffer(0x1000000);

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	EnterDataOutPhase();
}

int SCSIBR::GetMacAddr(vector<BYTE>& mac) const
{
	memcpy(mac.data(), mac_addr.data(), mac_addr.size());
	return (int)mac_addr.size();
}

void SCSIBR::SetMacAddr(const vector<BYTE>& mac)
{
	memcpy(mac_addr.data(), mac.data(), mac_addr.size());
}

void SCSIBR::ReceivePacket()
{
	// previous packet has not been received
	if (packet_enable) {
		return;
	}

	// Receive packet
	packet_len = tap.Receive(packet_buf.data());

	// Check if received packet
	if (memcmp(packet_buf.data(), mac_addr.data(), mac_addr.size()) != 0
				&& memcmp(packet_buf.data(), bcast_addr.data(), bcast_addr.size()) != 0) {
		packet_len = 0;
	}

	// Discard if it exceeds the buffer size
	if (packet_len > 2048) {
		packet_len = 0;
	}

	// Store in receive buffer
	if (packet_len > 0) {
		packet_enable = true;
	}
}

void SCSIBR::GetPacketBuf(vector<BYTE>& buf, int index)
{
	// Size limit
	int len = packet_len;
	if (len > 2048) {
		len = 2048;
	}

	// Copy
	memcpy(buf.data() + index, packet_buf.data(), len);

	// Received
	packet_enable = false;
}

void SCSIBR::SendPacket(const vector<BYTE>& buf, int len)
{
	tap.Send(buf.data(), len);
}

//---------------------------------------------------------------------------
//
//  $40 - Device Boot
//
//---------------------------------------------------------------------------
void SCSIBR::FS_InitDevice(vector<BYTE>& buf)
{
	fs.Reset();
	fsresult = fs.InitDevice((Human68k::argument_t*)buf.data());
}

//---------------------------------------------------------------------------
//
//  $41 - Directory Check
//
//---------------------------------------------------------------------------
void SCSIBR::FS_CheckDir(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	const int i = sizeof(uint32_t);

	const auto pNamests = (Human68k::namests_t*)&(buf.data()[i]);

	fsresult = fs.CheckDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $42 - Create Directory
//
//---------------------------------------------------------------------------
void SCSIBR::FS_MakeDir(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	const int i = sizeof(uint32_t);

	const auto pNamests = (Human68k::namests_t*)&(buf.data()[i]);

	fsresult = fs.MakeDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $43 - Remove Directory
//
//---------------------------------------------------------------------------
void SCSIBR::FS_RemoveDir(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	const int i = sizeof(uint32_t);

	const auto pNamests = (Human68k::namests_t*)&(buf.data()[i]);

	fsresult = fs.RemoveDir(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $44 - Rename
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Rename(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	const auto pNamests = (Human68k::namests_t*)&(buf.data()[i]);
	i += sizeof(Human68k::namests_t);

	const Human68k::namests_t *pNamestsNew = (Human68k::namests_t*)&(buf.data()[i]);

	fsresult = fs.Rename(nUnit, pNamests, pNamestsNew);
}

//---------------------------------------------------------------------------
//
//  $45 - Delete File
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Delete(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	const int i = sizeof(uint32_t);

	const auto *pNamests = (Human68k::namests_t*)&(buf.data()[i]);

	fsresult = fs.Delete(nUnit, pNamests);
}

//---------------------------------------------------------------------------
//
//  $46 - Get / Set file attributes
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Attribute(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	const auto pNamests = (Human68k::namests_t*)&(buf.data()[i]);
	i += sizeof(Human68k::namests_t);

	dp = (uint32_t*)&(buf.data()[i]);
	uint32_t nHumanAttribute = ntohl(*dp);

	fsresult = fs.Attribute(nUnit, pNamests, nHumanAttribute);
}

//---------------------------------------------------------------------------
//
//  $47 - File Search
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Files(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nKey = ntohl(*dp);
	i += sizeof(uint32_t);

	const auto pNamests = (Human68k::namests_t*)&(buf.data()[i]);
	i += sizeof(Human68k::namests_t);

	auto files = (Human68k::files_t*)&(buf.data()[i]);

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
void SCSIBR::FS_NFiles(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nKey = ntohl(*dp);
	i += sizeof(uint32_t);

	auto files = (Human68k::files_t*)&(buf.data()[i]);

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
void SCSIBR::FS_Create(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nKey = ntohl(*dp);
	i += sizeof(uint32_t);

	const auto pNamests = (Human68k::namests_t*)&(buf.data()[i]);
	i += sizeof(Human68k::namests_t);

	auto pFcb = (Human68k::fcb_t*)&(buf.data()[i]);
	i += sizeof(Human68k::fcb_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nAttribute = ntohl(*dp);
	i += sizeof(uint32_t);

	auto bp = (int*)&(buf.data()[i]);
	const uint32_t bForce = ntohl(*bp);

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
void SCSIBR::FS_Open(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nKey = ntohl(*dp);
	i += sizeof(uint32_t);

	const auto pNamests = (Human68k::namests_t*)&(buf.data()[i]);
	i += sizeof(Human68k::namests_t);

	auto pFcb = (Human68k::fcb_t*)&(buf.data()[i]);

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
void SCSIBR::FS_Close(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nKey = ntohl(*dp);
	i += sizeof(uint32_t);

	auto pFcb = (Human68k::fcb_t*)&(buf.data()[i]);

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
void SCSIBR::FS_Read(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nKey = ntohl(*dp);
	int i = sizeof(uint32_t);

	auto pFcb = (Human68k::fcb_t*)&(buf.data()[i]);
	i += sizeof(Human68k::fcb_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nSize = ntohl(*dp);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs.Read(nKey, pFcb, fsopt.data(), nSize);

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
void SCSIBR::FS_Write(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nKey = ntohl(*dp);
	int i = sizeof(uint32_t);

	auto pFcb = (Human68k::fcb_t*)&(buf.data()[i]);
	i += sizeof(Human68k::fcb_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nSize = ntohl(*dp);

	pFcb->fileptr = ntohl(pFcb->fileptr);
	pFcb->mode = ntohs(pFcb->mode);
	pFcb->time = ntohs(pFcb->time);
	pFcb->date = ntohs(pFcb->date);
	pFcb->size = ntohl(pFcb->size);

	fsresult = fs.Write(nKey, pFcb, fsopt.data(), nSize);

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
void SCSIBR::FS_Seek(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nKey = ntohl(*dp);
	int i = sizeof(uint32_t);

	auto pFcb = (Human68k::fcb_t*)&(buf.data()[i]);
	i += sizeof(Human68k::fcb_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nMode = ntohl(*dp);
	i += sizeof(uint32_t);

	auto ip = (const int*)&(buf.data()[i]);
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
void SCSIBR::FS_TimeStamp(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nKey = ntohl(*dp);
	i += sizeof(uint32_t);

	auto pFcb = (Human68k::fcb_t*)&(buf.data()[i]);
	i += sizeof(Human68k::fcb_t);

	dp = (uint32_t*)&(buf.data()[i]);
	uint32_t nHumanTime = ntohl(*dp);

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
void SCSIBR::FS_GetCapacity(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);

	Human68k::capacity_t cap;
	fsresult = fs.GetCapacity(nUnit, &cap);

	cap.freearea = htons(cap.freearea);
	cap.clusters = htons(cap.clusters);
	cap.sectors = htons(cap.sectors);
	cap.bytes = htons(cap.bytes);

	memcpy(fsout.data(), &cap, sizeof(Human68k::capacity_t));
	fsoutlen = sizeof(Human68k::capacity_t);
}

//---------------------------------------------------------------------------
//
//  $51 - Drive status inspection/control
//
//---------------------------------------------------------------------------
void SCSIBR::FS_CtrlDrive(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	const int i = sizeof(uint32_t);

	auto pCtrlDrive = (Human68k::ctrldrive_t*)&(buf.data()[i]);

	fsresult = fs.CtrlDrive(nUnit, pCtrlDrive);

	memcpy(fsout.data(), pCtrlDrive, sizeof(Human68k::ctrldrive_t));
	fsoutlen = sizeof(Human68k::ctrldrive_t);
}

//---------------------------------------------------------------------------
//
//  $52 - Get DPB
//
//---------------------------------------------------------------------------
void SCSIBR::FS_GetDPB(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);

	Human68k::dpb_t dpb;
	fsresult = fs.GetDPB(nUnit, &dpb);

	dpb.sector_size = htons(dpb.sector_size);
	dpb.fat_sector = htons(dpb.fat_sector);
	dpb.file_max = htons(dpb.file_max);
	dpb.data_sector = htons(dpb.data_sector);
	dpb.cluster_max = htons(dpb.cluster_max);
	dpb.root_sector = htons(dpb.root_sector);

	memcpy(fsout.data(), &dpb, sizeof(Human68k::dpb_t));
	fsoutlen = sizeof(Human68k::dpb_t);
}

//---------------------------------------------------------------------------
//
//  $53 - Read Sector
//
//---------------------------------------------------------------------------
void SCSIBR::FS_DiskRead(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nSector = ntohl(*dp);
	i += sizeof(uint32_t);

	dp = (uint32_t*)&(buf.data()[i]);
	uint32_t nSize = ntohl(*dp);

	fsresult = fs.DiskRead(nUnit, fsout.data(), nSector, nSize);
	fsoutlen = 0x200;
}

//---------------------------------------------------------------------------
//
//  $54 - Write Sector
//
//---------------------------------------------------------------------------
void SCSIBR::FS_DiskWrite(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);

	fsresult = fs.DiskWrite(nUnit);
}

//---------------------------------------------------------------------------
//
//  $55 - IOCTRL
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Ioctrl(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);
	int i = sizeof(uint32_t);

	dp = (uint32_t*)&(buf.data()[i]);
	const uint32_t nFunction = ntohl(*dp);
	i += sizeof(uint32_t);

	auto pIoctrl = (Human68k::ioctrl_t*)&(buf.data()[i]);

	switch (nFunction) {
		case 2:
		case (uint32_t)-2:
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
		case (uint32_t)-3:
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
void SCSIBR::FS_Flush(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);

	fsresult = fs.Flush(nUnit);
}

//---------------------------------------------------------------------------
//
//  $57 - Check Media
//
//---------------------------------------------------------------------------
void SCSIBR::FS_CheckMedia(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);

	fsresult = fs.CheckMedia(nUnit);
}

//---------------------------------------------------------------------------
//
//  $58 - Lock
//
//---------------------------------------------------------------------------
void SCSIBR::FS_Lock(vector<BYTE>& buf)
{
	auto dp = (uint32_t*)buf.data();
	const uint32_t nUnit = ntohl(*dp);

	fsresult = fs.Lock(nUnit);
}

//---------------------------------------------------------------------------
//
//	Read Filesystem (result code)
//
//---------------------------------------------------------------------------
int SCSIBR::ReadFsResult(vector<BYTE>& buf) const
{
	auto dp = (uint32_t *)buf.data();
	*dp = htonl(fsresult);
	return sizeof(uint32_t);
}

//---------------------------------------------------------------------------
//
//	Read Filesystem (return data)
//
//---------------------------------------------------------------------------
int SCSIBR::ReadFsOut(vector<BYTE>& buf) const
{
	copy_n(fsout.begin(), fsoutlen, buf.begin());
	return fsoutlen;
}

//---------------------------------------------------------------------------
//
//	Read file system (return option data)
//
//---------------------------------------------------------------------------
int SCSIBR::ReadFsOpt(vector<BYTE>& buf) const
{
	copy_n(fsopt.begin(), fsoptlen, buf.begin());
	return fsoptlen;
}

//---------------------------------------------------------------------------
//
//	Write Filesystem
//
//---------------------------------------------------------------------------
void SCSIBR::WriteFs(int func, vector<BYTE>& buf)
{
	fsresult = FS_FATAL_INVALIDCOMMAND;
	fsoutlen = 0;
	fsoptlen = 0;

	func &= 0x1f;
	switch (func) {
		case 0x00:
			FS_InitDevice(buf);	// $40 - start device
			break;
		case 0x01:
			FS_CheckDir(buf);	// $41 - directory check
			break;
		case 0x02:
			FS_MakeDir(buf);	// $42 - create directory
			break;
		case 0x03:
			FS_RemoveDir(buf);	// $43 - remove directory
			break;
		case 0x04:
			FS_Rename(buf);		// $44 - change file name
			break;
		case 0x05:
			FS_Delete(buf);		// $45 - delete file
			break;
		case 0x06:
			FS_Attribute(buf);	// $46 - Get/set file attribute
			break;
		case 0x07:
			FS_Files(buf);		// $47 - file search
			break;
		case 0x08:
			FS_NFiles(buf);		// $48 - next file search
			break;
		case 0x09:
			FS_Create(buf);		// $49 - create file
			break;
		case 0x0A:
			FS_Open(buf);		// $4A - File open
			break;
		case 0x0B:
			FS_Close(buf);		// $4B - File close
			break;
		case 0x0C:
			FS_Read(buf);		// $4C - read file
			break;
		case 0x0D:
			FS_Write(buf);		// $4D - write file
			break;
		case 0x0E:
			FS_Seek(buf);		// $4E - File seek
			break;
		case 0x0F:
			FS_TimeStamp(buf);	// $4F - Get/set file modification time
			break;
		case 0x10:
			FS_GetCapacity(buf);	// $50 - get capacity
			break;
		case 0x11:
			FS_CtrlDrive(buf);	// $51 - Drive control/state check
			break;
		case 0x12:
			FS_GetDPB(buf);		// $52 - Get DPB
			break;
		case 0x13:
			FS_DiskRead(buf);	// $53 - read sector
			break;
		case 0x14:
			FS_DiskWrite(buf);	// $54 - write sector
			break;
		case 0x15:
			FS_Ioctrl(buf);		// $55 - IOCTRL
			break;
		case 0x16:
			FS_Flush(buf);		// $56 - flush
			break;
		case 0x17:
			FS_CheckMedia(buf);	// $57 - check media exchange
			break;
		case 0x18:
			FS_Lock(buf);		// $58 - exclusive control
			break;
		default:
			break;
	}
}

//---------------------------------------------------------------------------
//
//	File system write (input option data)
//
//---------------------------------------------------------------------------
void SCSIBR::WriteFsOpt(const vector<BYTE>& buf, int num)
{
	copy_n(buf.begin(), num, fsopt.begin());
}
