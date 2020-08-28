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
#pragma once

#include "xm6.h"
#include "os.h"
#include "disk.h"

//===========================================================================
//
//	SCSI Host Bridge
//
//===========================================================================
#if defined(RASCSI) && !defined(BAREMETAL)
class CTapDriver;
#endif	// RASCSI && !BAREMETAL
class CFileSys;
class SCSIBR : public Disk
{
public:
	// Basic Functions
	SCSIBR();
										// Constructor
	virtual ~SCSIBR();
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

	int FASTCALL ReadFsResult(BYTE *buf);
										// Read filesystem (result code)
	int FASTCALL ReadFsOut(BYTE *buf);
										// Read filesystem (return data)
	int FASTCALL ReadFsOpt(BYTE *buf);
										// Read file system (optional data)
	void FASTCALL WriteFs(int func, BYTE *buf);
										// File system write (execute)
	void FASTCALL WriteFsOpt(BYTE *buf, int len);
										// File system write (optional data)
	// Command handlers
	void FASTCALL FS_InitDevice(BYTE *buf);
										// $40 - boot
	void FASTCALL FS_CheckDir(BYTE *buf);
										// $41 - directory check
	void FASTCALL FS_MakeDir(BYTE *buf);
										// $42 - create directory
	void FASTCALL FS_RemoveDir(BYTE *buf);
										// $43 - delete directory
	void FASTCALL FS_Rename(BYTE *buf);
										// $44 - change filename
	void FASTCALL FS_Delete(BYTE *buf);
										// $45 - delete file
	void FASTCALL FS_Attribute(BYTE *buf);
										// $46 - get/set file attributes
	void FASTCALL FS_Files(BYTE *buf);
										// $47 - file search
	void FASTCALL FS_NFiles(BYTE *buf);
										// $48 - find next file
	void FASTCALL FS_Create(BYTE *buf);
										// $49 - create file
	void FASTCALL FS_Open(BYTE *buf);
										// $4A - open file
	void FASTCALL FS_Close(BYTE *buf);
										// $4B - close file
	void FASTCALL FS_Read(BYTE *buf);
										// $4C - read file
	void FASTCALL FS_Write(BYTE *buf);
										// $4D - write file
	void FASTCALL FS_Seek(BYTE *buf);
										// $4E - seek file
	void FASTCALL FS_TimeStamp(BYTE *buf);
										// $4F - get/set file time
	void FASTCALL FS_GetCapacity(BYTE *buf);
										// $50 - get capacity
	void FASTCALL FS_CtrlDrive(BYTE *buf);
										// $51 - drive status check/control
	void FASTCALL FS_GetDPB(BYTE *buf);
										// $52 - get DPB
	void FASTCALL FS_DiskRead(BYTE *buf);
										// $53 - read sector
	void FASTCALL FS_DiskWrite(BYTE *buf);
										// $54 - write sector
	void FASTCALL FS_Ioctrl(BYTE *buf);
										// $55 - IOCTRL
	void FASTCALL FS_Flush(BYTE *buf);
										// $56 - flush cache
	void FASTCALL FS_CheckMedia(BYTE *buf);
										// $57 - check media
	void FASTCALL FS_Lock(BYTE *buf);
										// $58 - get exclusive control

	CFileSys *fs;
										// File system accessor
	DWORD fsresult;
										// File system access result code
	BYTE fsout[0x800];
										// File system access result buffer
	DWORD fsoutlen;
										// File system access result buffer size
	BYTE fsopt[0x1000000];
										// File system access buffer
	DWORD fsoptlen;
										// File system access buffer size
};
