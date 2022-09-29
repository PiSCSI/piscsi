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

#include "os.h"
#include "disk.h"
#include "ctapdriver.h"
#include "cfilesystem.h"
#include <string>
#include <array>

using namespace std; //NOSONAR Not relevant for rascsi

class SCSIBR final : public Disk
{
	static constexpr const array<BYTE, 6> bcast_addr = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

public:

	SCSIBR(int, int);
	~SCSIBR() override = default;

	bool Init(const unordered_map<string, string>&) override;
	bool Dispatch(scsi_command) override;

	// Commands
	vector<byte> InquiryInternal() const override;
	int GetMessage10(const vector<int>&, vector<BYTE>&);
	bool WriteBytes(const vector<int>&, vector<BYTE>&, uint64_t);
	void TestUnitReady() override;
	void GetMessage10();
	void SendMessage10();

private:

	using super = Disk;

	Dispatcher<SCSIBR> dispatcher;

	int GetMacAddr(vector<BYTE>&) const;		// Get MAC address
	void SetMacAddr(const vector<BYTE>&);		// Set MAC address
	void ReceivePacket();						// Receive a packet
	void GetPacketBuf(vector<BYTE>&, int);		// Get a packet
	void SendPacket(const vector<BYTE>&, int);	// Send a packet

	CTapDriver tap;								// TAP driver
	bool m_bTapEnable = false;					// TAP valid flag
	array<BYTE, 6> mac_addr = {};				// MAC Address
	int packet_len = 0;							// Receive packet size
	array<BYTE, 0x1000> packet_buf;				// Receive packet buffer
	bool packet_enable = false;					// Received packet valid

	int ReadFsResult(vector<BYTE>&) const;		// Read filesystem (result code)
	int ReadFsOut(vector<BYTE>&) const;			// Read filesystem (return data)
	int ReadFsOpt(vector<BYTE>&) const;			// Read file system (optional data)
	void WriteFs(int, vector<BYTE>&);			// File system write (execute)
	void WriteFsOpt(const vector<BYTE>&, int);	// File system write (optional data)

	// Command handlers
	void FS_InitDevice(vector<BYTE>&);				// $40 - boot
	void FS_CheckDir(vector<BYTE>&);				// $41 - directory check
	void FS_MakeDir(vector<BYTE>&);					// $42 - create directory
	void FS_RemoveDir(vector<BYTE>&);				// $43 - delete directory
	void FS_Rename(vector<BYTE>&);					// $44 - change filename
	void FS_Delete(vector<BYTE>&);					// $45 - delete file
	void FS_Attribute(vector<BYTE>&);				// $46 - get/set file attributes
	void FS_Files(vector<BYTE>&);					// $47 - file search
	void FS_NFiles(vector<BYTE>&);					// $48 - find next file
	void FS_Create(vector<BYTE>&);					// $49 - create file
	void FS_Open(vector<BYTE>&);					// $4A - open file
	void FS_Close(vector<BYTE>&);					// $4B - close file
	void FS_Read(vector<BYTE>&);					// $4C - read file
	void FS_Write(vector<BYTE>&);					// $4D - write file
	void FS_Seek(vector<BYTE>&);					// $4E - seek file
	void FS_TimeStamp(vector<BYTE>&);				// $4F - get/set file time
	void FS_GetCapacity(vector<BYTE>&);				// $50 - get capacity
	void FS_CtrlDrive(vector<BYTE>&);				// $51 - drive status check/control
	void FS_GetDPB(vector<BYTE>&);					// $52 - get DPB
	void FS_DiskRead(vector<BYTE>&);				// $53 - read sector
	void FS_DiskWrite(vector<BYTE>&);				// $54 - write sector
	void FS_Ioctrl(vector<BYTE>&);					// $55 - IOCTRL
	void FS_Flush(vector<BYTE>&);					// $56 - flush cache
	void FS_CheckMedia(vector<BYTE>&);				// $57 - check media
	void FS_Lock(vector<BYTE>&);					// $58 - get exclusive control

	CFileSys fs;								// File system accessor
	DWORD fsresult = 0;							// File system access result code
	array<BYTE, 0x800> fsout;					// File system access result buffer
	DWORD fsoutlen = 0;							// File system access result buffer size
	array<BYTE, 0x1000000> fsopt;				// File system access buffer
	DWORD fsoptlen = 0;							// File system access buffer size
};
