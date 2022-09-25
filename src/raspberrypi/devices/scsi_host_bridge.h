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

class SCSIBR final : public Disk
{
public:

	SCSIBR();
	~SCSIBR() override;
	SCSIBR(SCSIBR&) = delete;
	SCSIBR& operator=(const SCSIBR&) = delete;

	bool Init(const unordered_map<string, string>&) override;
	bool Dispatch(scsi_command) override;

	// Commands
	vector<byte> InquiryInternal() const override;
	int GetMessage10(const vector<int>&, BYTE *);
	bool WriteBytes(const vector<int>&, BYTE *, uint64_t);
	void TestUnitReady() override;
	void GetMessage10();
	void SendMessage10();

private:

	using super = Disk;

	Dispatcher<SCSIBR> dispatcher;

	int GetMacAddr(BYTE *buf) const;			// Get MAC address
	void SetMacAddr(const BYTE *buf);			// Set MAC address
	void ReceivePacket();						// Receive a packet
	void GetPacketBuf(BYTE *buf);				// Get a packet
	void SendPacket(const BYTE *buf, int len);	// Send a packet

	CTapDriver tap;								// TAP driver
	bool m_bTapEnable = false;					// TAP valid flag
	BYTE mac_addr[6];							// MAC Addres
	int packet_len = 0;							// Receive packet size
	BYTE packet_buf[0x1000];					// Receive packet buffer
	bool packet_enable = false;					// Received packet valid

	int ReadFsResult(BYTE *buf) const;			// Read filesystem (result code)
	int ReadFsOut(BYTE *buf) const;				// Read filesystem (return data)
	int ReadFsOpt(BYTE *buf) const;				// Read file system (optional data)
	void WriteFs(int func, BYTE *buf);			// File system write (execute)
	void WriteFsOpt(const BYTE *buf, int len);	// File system write (optional data)

	// Command handlers
	void FS_InitDevice(BYTE *buf);				// $40 - boot
	void FS_CheckDir(BYTE *buf);				// $41 - directory check
	void FS_MakeDir(BYTE *buf);					// $42 - create directory
	void FS_RemoveDir(BYTE *buf);				// $43 - delete directory
	void FS_Rename(BYTE *buf);					// $44 - change filename
	void FS_Delete(BYTE *buf);					// $45 - delete file
	void FS_Attribute(BYTE *buf);				// $46 - get/set file attributes
	void FS_Files(BYTE *buf);					// $47 - file search
	void FS_NFiles(BYTE *buf);					// $48 - find next file
	void FS_Create(BYTE *buf);					// $49 - create file
	void FS_Open(BYTE *buf);					// $4A - open file
	void FS_Close(BYTE *buf);					// $4B - close file
	void FS_Read(BYTE *buf);					// $4C - read file
	void FS_Write(BYTE *buf);					// $4D - write file
	void FS_Seek(BYTE *buf);					// $4E - seek file
	void FS_TimeStamp(BYTE *buf);				// $4F - get/set file time
	void FS_GetCapacity(BYTE *buf);				// $50 - get capacity
	void FS_CtrlDrive(BYTE *buf);				// $51 - drive status check/control
	void FS_GetDPB(BYTE *buf);					// $52 - get DPB
	void FS_DiskRead(BYTE *buf);				// $53 - read sector
	void FS_DiskWrite(BYTE *buf);				// $54 - write sector
	void FS_Ioctrl(BYTE *buf);					// $55 - IOCTRL
	void FS_Flush(BYTE *buf);					// $56 - flush cache
	void FS_CheckMedia(BYTE *buf);				// $57 - check media
	void FS_Lock(BYTE *buf);					// $58 - get exclusive control

	CFileSys fs;								// File system accessor
	DWORD fsresult = 0;							// File system access result code
	BYTE fsout[0x800];							// File system access result buffer
	DWORD fsoutlen = 0;							// File system access result buffer size
	BYTE fsopt[0x1000000];						// File system access buffer
	DWORD fsoptlen = 0;							// File system access buffer size
};
