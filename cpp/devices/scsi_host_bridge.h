//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  	Copyright (C) akuker
//
//  	Licensed under the BSD 3-Clause License.
//  	See LICENSE file in the project root folder.
//
//  	[ SCSI Host Bridge for the Sharp X68000 ]
//
//  Note: This requires a special driver on the host system and will only
//        work with the Sharp X68000 operating system.
//---------------------------------------------------------------------------
#pragma once

#include "interfaces/byte_writer.h"
#include "primary_device.h"
#include "ctapdriver.h"
#include "cfilesystem.h"
#include <string>
#include <array>

using namespace std;

class SCSIBR : public PrimaryDevice, public ByteWriter
{
	static constexpr const array<uint8_t, 6> bcast_addr = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

public:

	explicit SCSIBR(int lun) : PrimaryDevice(SCBR, lun) {}
	~SCSIBR() override = default;

	bool Init(const unordered_map<string, string>&) override;

	// Commands
	vector<uint8_t> InquiryInternal() const override;
	int GetMessage10(const vector<int>&, vector<uint8_t>&);
	bool WriteBytes(const vector<int>&, vector<uint8_t>&, uint32_t) override;
	void TestUnitReady() override;
	void GetMessage10();
	void SendMessage10() const;

private:

	int GetMacAddr(vector<uint8_t>&) const;		// Get MAC address
	void SetMacAddr(const vector<uint8_t>&);		// Set MAC address
	void ReceivePacket();						// Receive a packet
	void GetPacketBuf(vector<uint8_t>&, int);		// Get a packet
	void SendPacket(const vector<uint8_t>&, int);	// Send a packet

	CTapDriver tap;								// TAP driver
	bool m_bTapEnable = false;					// TAP valid flag
	array<uint8_t, 6> mac_addr = {};				// MAC Address
	int packet_len = 0;							// Receive packet size
	array<uint8_t, 0x1000> packet_buf;				// Receive packet buffer
	bool packet_enable = false;					// Received packet valid

	int ReadFsResult(vector<uint8_t>&) const;		// Read filesystem (result code)
	int ReadFsOut(vector<uint8_t>&) const;			// Read filesystem (return data)
	int ReadFsOpt(vector<uint8_t>&) const;			// Read file system (optional data)
	void WriteFs(int, vector<uint8_t>&);			// File system write (execute)
	void WriteFsOpt(const vector<uint8_t>&, int);	// File system write (optional data)

	// Command handlers
	void FS_InitDevice(vector<uint8_t>&);				// $40 - boot
	void FS_CheckDir(vector<uint8_t>&);				// $41 - directory check
	void FS_MakeDir(vector<uint8_t>&);					// $42 - create directory
	void FS_RemoveDir(vector<uint8_t>&);				// $43 - delete directory
	void FS_Rename(vector<uint8_t>&);					// $44 - change filename
	void FS_Delete(vector<uint8_t>&);					// $45 - delete file
	void FS_Attribute(vector<uint8_t>&);				// $46 - get/set file attributes
	void FS_Files(vector<uint8_t>&);					// $47 - file search
	void FS_NFiles(vector<uint8_t>&);					// $48 - find next file
	void FS_Create(vector<uint8_t>&);					// $49 - create file
	void FS_Open(vector<uint8_t>&);					// $4A - open file
	void FS_Close(vector<uint8_t>&);					// $4B - close file
	void FS_Read(vector<uint8_t>&);					// $4C - read file
	void FS_Write(vector<uint8_t>&);					// $4D - write file
	void FS_Seek(vector<uint8_t>&);					// $4E - seek file
	void FS_TimeStamp(vector<uint8_t>&);				// $4F - get/set file time
	void FS_GetCapacity(vector<uint8_t>&);				// $50 - get capacity
	void FS_CtrlDrive(vector<uint8_t>&);				// $51 - drive status check/control
	void FS_GetDPB(vector<uint8_t>&);					// $52 - get DPB
	void FS_DiskRead(vector<uint8_t>&);				// $53 - read sector
	void FS_DiskWrite(vector<uint8_t>&);				// $54 - write sector
	void FS_Ioctrl(vector<uint8_t>&);					// $55 - IOCTRL
	void FS_Flush(vector<uint8_t>&);					// $56 - flush cache
	void FS_CheckMedia(vector<uint8_t>&);				// $57 - check media
	void FS_Lock(vector<uint8_t>&);					// $58 - get exclusive control

	CFileSys fs;								// File system accessor
	uint32_t fsresult = 0;						// File system access result code
	array<uint8_t, 0x800> fsout;					// File system access result buffer
	uint32_t fsoutlen = 0;						// File system access result buffer size
	array<uint8_t, 0x1000000> fsopt;				// File system access buffer
	uint32_t fsoptlen = 0;						// File system access buffer size
};
