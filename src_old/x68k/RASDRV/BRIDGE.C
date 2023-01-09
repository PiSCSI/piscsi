//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technology.
//  Copyright (C) 2016-2017 GIMONS
//	[ Host Filesystem Bridge Driver ]
//
//---------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <sys/dos.h>
#include <sys/scsi.h>
#include <iocslib.h>
#include "bridge.h"

//---------------------------------------------------------------------------
//
//  Variable definitions
//
//---------------------------------------------------------------------------
volatile BYTE *request;				// Request header address
DWORD command;						// Command number
DWORD unit;							// Unit number

//===========================================================================
//
/// File system (SCSI integration)
//
//===========================================================================
typedef struct
{
	char DeviceType;
	char RMB;
	char ANSI_Ver;
	char RDF;
	char AddLen;
	char RESV0;
	char RESV1;
	char OptFunc;
	char VendorID[8];
	char ProductID[16];
	char FirmRev[4];
} INQUIRY_T;

int scsiid;	// SCSI ID

//---------------------------------------------------------------------------
//
// Initialize
//
//---------------------------------------------------------------------------
BOOL SCSI_Init(void)
{
	int i;
	INQUIRY_T inq;

	// SCSI ID not set
	scsiid = -1;
	
	for (i = 0; i <= 7; i++) {
		if (S_INQUIRY(sizeof(INQUIRY_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}

		if (memcmp(&(inq.ProductID), "RASCSI BRIDGE", 13) != 0) {
			continue;
		}

		// SCSI ID set
		scsiid = i;
		return TRUE;
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
// Send commands
//
//---------------------------------------------------------------------------
int SCSI_SendCmd(BYTE *buf, int len)
{
	int ret;
	BYTE cmdbuf[10];
	BYTE sts;
	BYTE msg;
	BYTE retbuf[4];
	
	ret = FS_FATAL_MEDIAOFFLINE;

	cmdbuf[0] = 0x2a;
	cmdbuf[1] = 0;
	cmdbuf[2] = 2;
	cmdbuf[3] = command;
	cmdbuf[4] = 0;
	cmdbuf[5] = 0;
	cmdbuf[6] = (BYTE)(len >> 16);
	cmdbuf[7] = (BYTE)(len >> 8);
	cmdbuf[8] = (BYTE)len;
	cmdbuf[9] = 0;

	// Retry when select times out
	if (S_SELECT(scsiid) != 0) {
		S_RESET();
		if (S_SELECT(scsiid) != 0) {
			return ret;
		}
	}

	if (S_CMDOUT(10, cmdbuf) != 0) {
		return ret;
	}

	if (S_DATAOUT_P(len, buf) != 0) {
		return ret;
	}

	if (S_STSIN(&sts) != 0) {
		return ret;
	}

	if (S_MSGIN(&msg) != 0) {
		return ret;
	}

	cmdbuf[0] = 0x28;
	cmdbuf[1] = 0;
	cmdbuf[2] = 2;
	cmdbuf[3] = command;
	cmdbuf[4] = 0;
	cmdbuf[5] = 0;
	cmdbuf[6] = 0;
	cmdbuf[7] = 0;
	cmdbuf[8] = 4;
	cmdbuf[9] = 0;

	// Retry when select times out
	if (S_SELECT(scsiid) != 0) {
		S_RESET();
		if (S_SELECT(scsiid) != 0) {
			return ret;
		}
	}

	if (S_CMDOUT(10, cmdbuf) != 0) {
		return ret;
	}

	if (S_DATAIN_P(4, retbuf) != 0) {
		return ret;
	}

	if (S_STSIN(&sts) != 0) {
		return ret;
	}

	if (S_MSGIN(&msg) != 0) {
		return ret;
	}
	
	ret = *(int*)retbuf;
	return ret;
}

//---------------------------------------------------------------------------
//
// Call commands
//
//---------------------------------------------------------------------------
int SCSI_CalCmd(BYTE *buf, int len, BYTE *outbuf, int outlen)
{
	int ret;
	BYTE cmdbuf[10];
	BYTE sts;
	BYTE msg;
	BYTE retbuf[4];
	
	ret = FS_FATAL_MEDIAOFFLINE;

	cmdbuf[0] = 0x2a;
	cmdbuf[1] = 0;
	cmdbuf[2] = 2;
	cmdbuf[3] = command;
	cmdbuf[4] = 0;
	cmdbuf[5] = 0;
	cmdbuf[6] = (BYTE)(len >> 16);
	cmdbuf[7] = (BYTE)(len >> 8);
	cmdbuf[8] = (BYTE)len;
	cmdbuf[9] = 0;
	
	// Retry when select times out
	if (S_SELECT(scsiid) != 0) {
		S_RESET();
		if (S_SELECT(scsiid) != 0) {
			return ret;
		}
	}

	if (S_CMDOUT(10, cmdbuf) != 0) {
		return ret;
	}

	if (S_DATAOUT_P(len, buf) != 0) {
		return ret;
	}

	if (S_STSIN(&sts) != 0) {
		return ret;
	}

	if (S_MSGIN(&msg) != 0) {
		return ret;
	}

	cmdbuf[0] = 0x28;
	cmdbuf[1] = 0;
	cmdbuf[2] = 2;
	cmdbuf[3] = command;
	cmdbuf[4] = 0;
	cmdbuf[5] = 0;
	cmdbuf[6] = 0;
	cmdbuf[7] = 0;
	cmdbuf[8] = 4;
	cmdbuf[9] = 0;

	// Retry when select times out
	if (S_SELECT(scsiid) != 0) {
		S_RESET();
		if (S_SELECT(scsiid) != 0) {
			return ret;
		}
	}

	if (S_CMDOUT(10, cmdbuf) != 0) {
		return ret;
	}

	if (S_DATAIN_P(4, retbuf) != 0) {
		return ret;
	}

	if (S_STSIN(&sts) != 0) {
		return ret;
	}

	if (S_MSGIN(&msg) != 0) {
		return ret;
	}

	// Stop receiving return data on error
	ret = *(int*)retbuf;
	if (ret < 0) {
		return ret;
	}

	cmdbuf[0] = 0x28;
	cmdbuf[1] = 0;
	cmdbuf[2] = 2;
	cmdbuf[3] = command;
	cmdbuf[4] = 0;
	cmdbuf[5] = 0;
	cmdbuf[6] = (BYTE)(outlen >> 16);
	cmdbuf[7] = (BYTE)(outlen >> 8);
	cmdbuf[8] = (BYTE)outlen;
	cmdbuf[9] = 1;

	// Retry when select times out
	if (S_SELECT(scsiid) != 0) {
		S_RESET();
		if (S_SELECT(scsiid) != 0) {
			return ret;
		}
	}

	if (S_CMDOUT(10, cmdbuf) != 0) {
		return ret;
	}

	if (S_DATAIN_P(outlen, outbuf) != 0) {
		return ret;
	}

	if (S_STSIN(&sts) != 0) {
		return ret;
	}

	if (S_MSGIN(&msg) != 0) {
		return ret;
	}
	
	ret = *(int*)retbuf;
	return ret;
}

//---------------------------------------------------------------------------
//
// Get option data
//
//---------------------------------------------------------------------------
BOOL SCSI_ReadOpt(BYTE *buf, int len)
{
	BYTE cmdbuf[10];
	BYTE sts;
	BYTE msg;

	cmdbuf[0] = 0x28;
	cmdbuf[1] = 0;
	cmdbuf[2] = 2;
	cmdbuf[3] = command;
	cmdbuf[4] = 0;
	cmdbuf[5] = 0;
	cmdbuf[6] = (BYTE)(len >> 16);
	cmdbuf[7] = (BYTE)(len >> 8);
	cmdbuf[8] = (BYTE)len;
	cmdbuf[9] = 2;

	// Retry when select times out
	if (S_SELECT(scsiid) != 0) {
		S_RESET();
		if (S_SELECT(scsiid) != 0) {
			return FALSE;
		}
	}

	if (S_CMDOUT(10, cmdbuf) != 0) {
		return FALSE;
	}

	if (S_DATAIN_P(len, buf) != 0) {
		return FALSE;
	}

	if (S_STSIN(&sts) != 0) {
		return FALSE;
	}

	if (S_MSGIN(&msg) != 0) {
		return FALSE;
	}
	
	return TRUE;
}

//---------------------------------------------------------------------------
//
// Write option data
//
//---------------------------------------------------------------------------
BOOL SCSI_WriteOpt(BYTE *buf, int len)
{
	BYTE cmdbuf[10];
	BYTE sts;
	BYTE msg;

	cmdbuf[0] = 0x2a;
	cmdbuf[1] = 0;
	cmdbuf[2] = 2;
	cmdbuf[3] = command;
	cmdbuf[4] = 0;
	cmdbuf[5] = 0;
	cmdbuf[6] = (BYTE)(len >> 16);
	cmdbuf[7] = (BYTE)(len >> 8);
	cmdbuf[8] = (BYTE)len;
	cmdbuf[9] = 1;

	// Retry when select times out
	if (S_SELECT(scsiid) != 0) {
		S_RESET();
		if (S_SELECT(scsiid) != 0) {
			return FALSE;
		}
	}

	if (S_CMDOUT(10, cmdbuf) != 0) {
		return FALSE;
	}

	if (S_DATAOUT_P(len, buf) != 0) {
		return FALSE;
	}

	if (S_STSIN(&sts) != 0) {
		return FALSE;
	}

	if (S_MSGIN(&msg) != 0) {
		return FALSE;
	}

	return TRUE;
}

//===========================================================================
//
/// File System
//
//===========================================================================

//---------------------------------------------------------------------------
//
//  $40 - Device startup
//
//---------------------------------------------------------------------------
DWORD FS_InitDevice(const argument_t* pArgument)
{
	return (DWORD)SCSI_SendCmd((BYTE*)pArgument, sizeof(argument_t));
}

//---------------------------------------------------------------------------
//
//  $41 - Directory check
//
//---------------------------------------------------------------------------
int FS_CheckDir(const namests_t* pNamests)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	memcpy(&buf[i], pNamests, sizeof(namests_t));
	i += sizeof(namests_t);
	
	return SCSI_SendCmd(buf, i);
}

//---------------------------------------------------------------------------
//
//  $42 - Create directory
//
//---------------------------------------------------------------------------
int FS_MakeDir(const namests_t* pNamests)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	memcpy(&buf[i], pNamests, sizeof(namests_t));
	i += sizeof(namests_t);
	
	return SCSI_SendCmd(buf, i);
}

//---------------------------------------------------------------------------
//
//  $43 - Delete directory
//
//---------------------------------------------------------------------------
int FS_RemoveDir(const namests_t* pNamests)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	memcpy(&buf[i], pNamests, sizeof(namests_t));
	i += sizeof(namests_t);
	
	return SCSI_SendCmd(buf, i);
}

//---------------------------------------------------------------------------
//
//  $44 - Change file name
//
//---------------------------------------------------------------------------
int FS_Rename(const namests_t* pNamests, const namests_t* pNamestsNew)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	memcpy(&buf[i], pNamests, sizeof(namests_t));
	i += sizeof(namests_t);

	memcpy(&buf[i], pNamestsNew, sizeof(namests_t));
	i += sizeof(namests_t);
	
	return SCSI_SendCmd(buf, i);
}

//---------------------------------------------------------------------------
//
//  $45 - Delete file
//
//---------------------------------------------------------------------------
int FS_Delete(const namests_t* pNamests)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	memcpy(&buf[i], pNamests, sizeof(namests_t));
	i += sizeof(namests_t);
	
	return SCSI_SendCmd(buf, i);
}

//---------------------------------------------------------------------------
//
//  $46 - Get/set file attribute
//
//---------------------------------------------------------------------------
int FS_Attribute(const namests_t* pNamests, DWORD nHumanAttribute)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	memcpy(&buf[i], pNamests, sizeof(namests_t));
	i += sizeof(namests_t);

	dp = (DWORD*)&buf[i];
	*dp = nHumanAttribute;
	i += sizeof(DWORD);
	
	return SCSI_SendCmd(buf, i);
}

//---------------------------------------------------------------------------
//
//  $47 - File search
//
//---------------------------------------------------------------------------
int FS_Files(DWORD nKey,
	const namests_t* pNamests, files_t* info)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nKey;
	i += sizeof(DWORD);

	memcpy(&buf[i], pNamests, sizeof(namests_t));
	i += sizeof(namests_t);

	memcpy(&buf[i], info, sizeof(files_t));
	i += sizeof(files_t);
	
	return SCSI_CalCmd(buf, i, (BYTE*)info, sizeof(files_t));
}

//---------------------------------------------------------------------------
//
//  $48 - Search next file
//
//---------------------------------------------------------------------------
int FS_NFiles(DWORD nKey, files_t* info)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nKey;
	i += sizeof(DWORD);

	memcpy(&buf[i], info, sizeof(files_t));
	i += sizeof(files_t);
	
	return SCSI_CalCmd(buf, i, (BYTE*)info, sizeof(files_t));
}

//---------------------------------------------------------------------------
//
//  $49 - Create new file
//
//---------------------------------------------------------------------------
int FS_Create(DWORD nKey,
	const namests_t* pNamests, fcb_t* pFcb, DWORD nAttribute, BOOL bForce)
{
	BYTE buf[256];
	DWORD *dp;
	BOOL *bp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nKey;
	i += sizeof(DWORD);

	memcpy(&buf[i], pNamests, sizeof(namests_t));
	i += sizeof(namests_t);
	
	memcpy(&buf[i], pFcb, sizeof(fcb_t));
	i += sizeof(fcb_t);

	dp = (DWORD*)&buf[i];
	*dp = nAttribute;
	i += sizeof(DWORD);

	bp = (BOOL*)&buf[i];
	*bp = bForce;
	i += sizeof(BOOL);
	
	return SCSI_CalCmd(buf, i, (BYTE*)pFcb, sizeof(fcb_t));
}

//---------------------------------------------------------------------------
//
//  $4A - File open
//
//---------------------------------------------------------------------------
int FS_Open(DWORD nKey,
	const namests_t* pNamests, fcb_t* pFcb)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nKey;
	i += sizeof(DWORD);

	memcpy(&buf[i], pNamests, sizeof(namests_t));
	i += sizeof(namests_t);
	
	memcpy(&buf[i], pFcb, sizeof(fcb_t));
	i += sizeof(fcb_t);

	return SCSI_CalCmd(buf, i, (BYTE*)pFcb, sizeof(fcb_t));
}

//---------------------------------------------------------------------------
//
//  $4B - File close
//
//---------------------------------------------------------------------------
int FS_Close(DWORD nKey, fcb_t* pFcb)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nKey;
	i += sizeof(DWORD);
	
	memcpy(&buf[i], pFcb, sizeof(fcb_t));
	i += sizeof(fcb_t);

	return SCSI_CalCmd(buf, i, (BYTE*)pFcb, sizeof(fcb_t));
}

//---------------------------------------------------------------------------
//
//  $4C - Read file
//
//---------------------------------------------------------------------------
int FS_Read(DWORD nKey, fcb_t* pFcb, BYTE* pAddress, DWORD nSize)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	int nResult;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = nKey;
	i += sizeof(DWORD);

	memcpy(&buf[i], pFcb, sizeof(fcb_t));
	i += sizeof(fcb_t);

	dp = (DWORD*)&buf[i];
	*dp = nSize;
	i += sizeof(DWORD);
	
	nResult = SCSI_CalCmd(buf, i, (BYTE*)pFcb, sizeof(fcb_t));

	if (nResult > 0) {
		SCSI_ReadOpt(pAddress, nResult);
	}
	
	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4D - Write file
//
//---------------------------------------------------------------------------
int FS_Write(DWORD nKey, fcb_t* pFcb, BYTE* pAddress, DWORD nSize)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = nKey;
	i += sizeof(DWORD);

	memcpy(&buf[i], pFcb, sizeof(fcb_t));
	i += sizeof(fcb_t);
	
	dp = (DWORD*)&buf[i];
	*dp = nSize;
	i += sizeof(DWORD);

	if (nSize != 0) {
		if (!SCSI_WriteOpt(pAddress, nSize)) {
			return FS_FATAL_MEDIAOFFLINE;
		}
	}
	
	return SCSI_SendCmd(buf, i);
}

//---------------------------------------------------------------------------
//
//  $4E - File seek
//
//---------------------------------------------------------------------------
int FS_Seek(DWORD nKey, fcb_t* pFcb, DWORD nMode, int nOffset)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = nKey;
	i += sizeof(DWORD);

	memcpy(&buf[i], pFcb, sizeof(fcb_t));
	i += sizeof(fcb_t);

	dp = (DWORD*)&buf[i];
	*dp = nMode;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nOffset;
	i += sizeof(int);
	
	return SCSI_CalCmd(buf, i, (BYTE*)pFcb, sizeof(fcb_t));
}

//---------------------------------------------------------------------------
//
//  $4F - Get/set file time stamp
//
//---------------------------------------------------------------------------
DWORD FS_TimeStamp(DWORD nKey,
	fcb_t* pFcb, DWORD nHumanTime)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nKey;
	i += sizeof(DWORD);

	memcpy(&buf[i], pFcb, sizeof(fcb_t));
	i += sizeof(fcb_t);

	dp = (DWORD*)&buf[i];
	*dp = nHumanTime;
	i += sizeof(DWORD);
	
	return (DWORD)SCSI_CalCmd(buf, i, (BYTE*)pFcb, sizeof(fcb_t));
}

//---------------------------------------------------------------------------
//
//  $50 - Get capacity
//
//---------------------------------------------------------------------------
int FS_GetCapacity(capacity_t* cap)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	return SCSI_CalCmd(buf, i, (BYTE*)cap, sizeof(capacity_t));
}

//---------------------------------------------------------------------------
//
//  $51 - Inspect/control drive status
//
//---------------------------------------------------------------------------
int FS_CtrlDrive(ctrldrive_t* pCtrlDrive)
{
#if 1
	// Do tentative processing here due to high load
	switch (pCtrlDrive->status) {
		case 0:		// Inspect status
		case 9:		// Inspect status 2
			pCtrlDrive->status = 0x42;
			return pCtrlDrive->status;
		case 1:		// Eject
		case 2:		// Eject forbidden 1 (not implemented)
		case 3:		// Eject allowed 1 (not implemented)
		case 4:		// Flash LED when media is not inserted (not implemented)
		case 5:		// Turn off LED when media is not inserted (not implemented)
		case 6:		// Eject forbidden 2 (not implemented)
		case 7:		// Eject allowed 2 (not implemented)
			return 0;

		case 8:		// Eject inspection
			return 1;
	}

	return -1;
#else
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	memcpy(&buf[i], pCtrlDrive, sizeof(ctrldrive_t));
	i += sizeof(ctrldrive_t);
	
	return SCSI_CalCmd(buf, i, (BYTE*)pCtrlDrive, sizeof(ctrldrive_t));
#endif
}

//---------------------------------------------------------------------------
//
//  $52 - Get DPB
//
//---------------------------------------------------------------------------
int FS_GetDPB(dpb_t* pDpb)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	return SCSI_CalCmd(buf, i, (BYTE*)pDpb, sizeof(dpb_t));
}

//---------------------------------------------------------------------------
//
//  $53 - Read sector
//
//---------------------------------------------------------------------------
int FS_DiskRead(BYTE* pBuffer, DWORD nSector, DWORD nSize)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nSector;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nSize;
	i += sizeof(DWORD);
	
	return SCSI_CalCmd(buf, i, (BYTE*)pBuffer, 0x200);
}

//---------------------------------------------------------------------------
//
//  $54 - Write sector
//
//---------------------------------------------------------------------------
int FS_DiskWrite()
{
	BYTE buf[256];
	DWORD *dp;
	
	dp = (DWORD*)buf;
	*dp = unit;

	return SCSI_SendCmd(buf, 4);
}

//---------------------------------------------------------------------------
//
//  $55 - IOCTRL
//
//---------------------------------------------------------------------------
int FS_Ioctrl(DWORD nFunction, ioctrl_t* pIoctrl)
{
	BYTE buf[256];
	DWORD *dp;
	int i;
	
	i = 0;
	dp = (DWORD*)buf;
	*dp = unit;
	i += sizeof(DWORD);

	dp = (DWORD*)&buf[i];
	*dp = nFunction;
	i += sizeof(DWORD);

	memcpy(&buf[i], pIoctrl, sizeof(ioctrl_t));
	i += sizeof(ioctrl_t);
	
	return SCSI_CalCmd(buf, i, (BYTE*)pIoctrl, sizeof(ioctrl_t));
}

//---------------------------------------------------------------------------
//
//  $56 - Flush
//
//---------------------------------------------------------------------------
int FS_Flush()
{
#if 1
	// Not supported
	return 0;
#else
	BYTE buf[256];
	DWORD *dp;
	
	dp = (DWORD*)buf;
	*dp = unit;

	return SCSI_SendCmd(buf, 4);
#endif
}

//---------------------------------------------------------------------------
//
//  $57 - Media change check
//
//---------------------------------------------------------------------------
int FS_CheckMedia()
{
#if 1
	// Do tentative processing due to high load
	return 0;
#else
	BYTE buf[256];
	DWORD *dp;
	
	dp = (DWORD*)buf;
	*dp = unit;

	return SCSI_SendCmd(buf, 4);
#endif
}

//---------------------------------------------------------------------------
//
//  $58 - Lock
//
//---------------------------------------------------------------------------
int FS_Lock()
{
#if 1
	// Not supported
	return 0;
#else
	BYTE buf[256];
	DWORD *dp;
	
	dp = (DWORD*)buf;
	*dp = unit;

	return SCSI_SendCmd(buf, 4);
#endif
}

//===========================================================================
//
//	Command handlers
//
//===========================================================================
#define GetReqByte(a) (request[a])
#define SetReqByte(a,d) (request[a] = (d))
#define GetReqWord(a) (*((WORD*)&request[a]))
#define SetReqWord(a,d) (*((WORD*)&request[a]) = (d))
#define GetReqLong(a) (*((DWORD*)&request[a]))
#define SetReqLong(a,d) (*((DWORD*)&request[a]) = (d))
#define GetReqAddr(a) ((BYTE*)((*((DWORD*)&request[a])) & 0x00ffffff))

//---------------------------------------------------------------------------
//
//  Read NAMESTS
//
//---------------------------------------------------------------------------
void GetNameStsPath(BYTE *addr, namests_t* pNamests)
{
	DWORD i;

	ASSERT(this);
	ASSERT(pNamests);

	// Wildcard data
	pNamests->wildcard = *addr;

	// Drive number
	pNamests->drive = addr[1];

	// Pathname
	for (i = 0; i < sizeof(pNamests->path); i++) {
		pNamests->path[i] = addr[2 + i];
	}

	// Filename 1
	memset(pNamests->name, 0x20, sizeof(pNamests->name));

	// File ending
	memset(pNamests->ext, 0x20, sizeof(pNamests->ext));

	// Filename 2
	memset(pNamests->add, 0, sizeof(pNamests->add));
}

//---------------------------------------------------------------------------
//
//  Read NAMESTS
//
//---------------------------------------------------------------------------
void GetNameSts(BYTE *addr, namests_t* pNamests)
{
	DWORD i;

	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(addr <= 0xFFFFFF);

	// Wildcard data
	pNamests->wildcard = *addr;

	// Drive number
	pNamests->drive = addr[1];

	// Pathname
	for (i = 0; i < sizeof(pNamests->path); i++) {
		pNamests->path[i] = addr[2 + i];
	}

	// Filename 1
	for (i = 0; i < sizeof(pNamests->name); i++) {
		pNamests->name[i] = addr[67 + i];
	}

	// File ending
	for (i = 0; i < sizeof(pNamests->ext); i++) {
		pNamests->ext[i] = addr[75 + i];
	}

	// Filename 2
	for (i = 0; i < sizeof(pNamests->add); i++) {
		pNamests->add[i] = addr[78 + i];
	}
}

//---------------------------------------------------------------------------
//
//  Read FILES
//
//---------------------------------------------------------------------------
void GetFiles(BYTE *addr, files_t* pFiles)
{
	ASSERT(this);
	ASSERT(pFiles);
	ASSERT(addr <= 0xFFFFFF);

	// Search data
	pFiles->fatr = *addr;
	pFiles->sector = *((DWORD*)&addr[2]);
	pFiles->offset = *((WORD*)&addr[8]);;

	pFiles->attr = 0;
	pFiles->time = 0;
	pFiles->date = 0;
	pFiles->size = 0;
	memset(pFiles->full, 0, sizeof(pFiles->full));
}

//---------------------------------------------------------------------------
//
//  Write FILES
//
//---------------------------------------------------------------------------
void SetFiles(BYTE *addr, const files_t* pFiles)
{
	DWORD i;

	ASSERT(this);
	ASSERT(pFiles);

	*((DWORD*)&addr[2]) = pFiles->sector;
	*((WORD*)&addr[8]) = pFiles->offset;

	// File data
	addr[21] = pFiles->attr;
	*((WORD*)&addr[22]) = pFiles->time;
	*((WORD*)&addr[24]) = pFiles->date;
	*((DWORD*)&addr[26]) = pFiles->size;

	// Full filename
	addr += 30;
	for (i = 0; i < sizeof(pFiles->full); i++) {
		*addr = pFiles->full[i];
		addr++;
	}
}

//---------------------------------------------------------------------------
//
//  Read FCB
//
//---------------------------------------------------------------------------
void GetFcb(BYTE *addr, fcb_t* pFcb)
{
	ASSERT(this);
	ASSERT(pFcb);

	// FCB data
	pFcb->fileptr = *((DWORD*)&addr[6]);
	pFcb->mode = *((WORD*)&addr[14]);

	// Attribute
	pFcb->attr = addr[47];

	// FCB data
	pFcb->time = *((WORD*)&addr[58]);
	pFcb->date = *((WORD*)&addr[60]);
	pFcb->size = *((DWORD*)&addr[64]);
}

//---------------------------------------------------------------------------
//
//  Write FCB
//
//---------------------------------------------------------------------------
void SetFcb(BYTE *addr, const fcb_t* pFcb)
{
	ASSERT(this);
	ASSERT(pFcb);

	// FCB data
	*((DWORD*)&addr[6]) = pFcb->fileptr;
	*((WORD*)&addr[14]) = pFcb->mode;

	// Attribute
	addr[47] = pFcb->attr;

	// FCB data
	*((WORD*)&addr[58]) = pFcb->time;
	*((WORD*)&addr[60]) = pFcb->date;
	*((DWORD*)&addr[64]) = pFcb->size;
}

//---------------------------------------------------------------------------
//
//  Write CAPACITY
//
//---------------------------------------------------------------------------
void SetCapacity(BYTE *addr, const capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(pCapacity);

	*((WORD*)&addr[0]) = pCapacity->freearea;
	*((WORD*)&addr[2]) = pCapacity->clusters;
	*((WORD*)&addr[4]) = pCapacity->sectors;
	*((WORD*)&addr[6]) = pCapacity->bytes;
}

//---------------------------------------------------------------------------
//
//  Write DPB
//
//---------------------------------------------------------------------------
void SetDpb(BYTE *addr, const dpb_t* pDpb)
{
	ASSERT(this);
	ASSERT(pDpb);

	// DPB data
	*((WORD*)&addr[0]) = pDpb->sector_size;
	addr[2] = 	pDpb->cluster_size;
	addr[3] = 	pDpb->shift;
	*((WORD*)&addr[4]) = pDpb->fat_sector;
	addr[6] = 	pDpb->fat_max;
	addr[7] = 	pDpb->fat_size;
	*((WORD*)&addr[8]) = pDpb->file_max;
	*((WORD*)&addr[10]) = pDpb->data_sector;
	*((WORD*)&addr[12]) = pDpb->cluster_max;
	*((WORD*)&addr[14]) = pDpb->root_sector;
	addr[20] = 	pDpb->media;
}

//---------------------------------------------------------------------------
//
//  Read IOCTRL
//
//---------------------------------------------------------------------------
void GetIoctrl(DWORD param, DWORD func, ioctrl_t* pIoctrl)
{
	DWORD *lp;
	
	ASSERT(this);
	ASSERT(pIoctrl);

	switch (func) {
		case 2:
			// Re-identify media
			pIoctrl->param = param;
			return;

		case -2:
			// Configure options
			lp = (DWORD*)param;
			pIoctrl->param = *lp;
			return;
	}
}

//---------------------------------------------------------------------------
//
//  Write IOCTRL
//
//---------------------------------------------------------------------------
void SetIoctrl(DWORD param, DWORD func, ioctrl_t* pIoctrl)
{
	DWORD i;
	BYTE *bp;
	WORD *wp;
	DWORD *lp;

	ASSERT(this);
	ASSERT(pIoctrl);

	switch (func) {
		case 0:
			// Acquire media ID
			wp = (WORD*)param;
			*wp = pIoctrl->media;
			return;

		case 1:
			// Dummy for Human68k compatibility
			lp = (DWORD*)param;
			*lp = pIoctrl->param;
			return;

		case -1:
			// Resident evaluation
			bp = (BYTE*)param;
			for (i = 0; i < 8; i++) {
				*bp = pIoctrl->buffer[i];
				bp++;
			}
			return;

		case -3:
			// Acquire options
			lp = (DWORD*)param;
			*lp = pIoctrl->param;
			return;
	}
}

//---------------------------------------------------------------------------
//
//  Read ARGUMENT
// 
//  When this exceeds the buffer size, interrupt transfer and exit.
//
//---------------------------------------------------------------------------
void GetArgument(BYTE *addr, argument_t* pArgument)
{
	BOOL bMode;
	BYTE *p;
	DWORD i;
	BYTE c;

	ASSERT(this);
	ASSERT(pArgument);

	bMode = FALSE;
	p = pArgument->buf;
	for (i = 0; i < sizeof(pArgument->buf) - 2; i++) {
		c = addr[i];
		*p++ = c;
		if (bMode == 0) {
			if (c == '\0')
				return;
			bMode = TRUE;
		} else {
			if (c == '\0')
				bMode = FALSE;
		}
	}

	*p++ = '\0';
	*p = '\0';
}

//---------------------------------------------------------------------------
//
//  Write return value
//
//---------------------------------------------------------------------------
void SetResult(DWORD nResult)
{
	DWORD code;

	ASSERT(this);

	// Handle fatal errors
	switch (nResult) {
		case FS_FATAL_INVALIDUNIT:
			code = 0x5001;
			goto fatal;
		case FS_FATAL_INVALIDCOMMAND:
			code = 0x5003;
			goto fatal;
		case FS_FATAL_WRITEPROTECT:
			code = 0x700D;
			goto fatal;
		case FS_FATAL_MEDIAOFFLINE:
			code = 0x7002;
		fatal:
			SetReqByte(3, (BYTE)code);
			SetReqByte(4, code >> 8);
			//  @note When returning retryability, never overwrite with (a5 + 18).
			//  If you do that, the system will start behaving erratically if you hit Retry at the system error screen.
			if (code & 0x2000)
				break;
			nResult = FS_INVALIDFUNC;
		default:
			SetReqLong(18, nResult);
			break;
	}
}

//---------------------------------------------------------------------------
//
//  $40 - Device startup
// 
//	in	(offset	size)
//		 0	1.b	constant (22)
//		 2	1.b	command ($40/$c0)
//		18	1.l	parameter address
//		22	1.b	drive number
//	out	(offset	size)
//		 3	1.b	error code (lower)
//		 4	1.b	''			(upper)
//		13	1.b	number of units
//		14	1.l	device driver exit address + 1
//
//  This is called command 0 when the driver is loaded similarly to a local drive,
//  but there is no need to prepare BPB or its pointer array.
//  Unlike other commands, only this command does not include a valid a5 + 1,
//  since it doesn't expect 0 initialization etc., so be careful.
//
//---------------------------------------------------------------------------
DWORD InitDevice(void)
{
	argument_t arg;
	DWORD units;

	ASSERT(this);
	ASSERT(fs);

	// Get option contents
	GetArgument(GetReqAddr(18), &arg);

	// Contstruct the filesystem to the extent Human68k can support number of drivers etc.
	units = FS_InitDevice(&arg);

	// Return number of drives
	SetReqByte(13, units);

	return 0;
}

//---------------------------------------------------------------------------
//
//  $41 - Directory check
// 
//	in	(offset	size)
//		14	1.L	NAMESTS struct address
//
//---------------------------------------------------------------------------
DWORD CheckDir(void)
{
	namests_t ns;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get filename to search
	GetNameStsPath(GetReqAddr(14), &ns);

	// Call filesystem
	nResult = FS_CheckDir(&ns);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $42 - Create directory
// 
//	in	(offset	size)
//		14	1.L	NAMESTS struct address
//
//---------------------------------------------------------------------------
DWORD MakeDir(void)
{
	namests_t ns;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get filename to search
	GetNameSts(GetReqAddr(14), &ns);

	// Call filesystem
	nResult = FS_MakeDir(&ns);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $43 - Delete directory
// 
//	in	(offset	size)
//		14	1.L	NAMESTS struct address
//
//---------------------------------------------------------------------------
DWORD RemoveDir(void)
{
	namests_t ns;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get filename to search
	GetNameSts(GetReqAddr(14), &ns);

	// Call filesystem
	nResult = FS_RemoveDir(&ns);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $44 - Change filename
// 
//	in	(offset	size)
//		14	1.L	NAMESTS struct address old filename
//		18	1.L	NAMESTS struct address new filename
//
//---------------------------------------------------------------------------
DWORD Rename(void)
{
	namests_t ns;
	namests_t ns_new;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get filename to search
	GetNameSts(GetReqAddr(14), &ns);
	GetNameSts(GetReqAddr(18), &ns_new);

	// Call filesystem
	nResult = FS_Rename(&ns, &ns_new);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $45 - Delete file
// 
//	in	(offset	size)
//		14	1.L	NAMESTS struct address
//
//---------------------------------------------------------------------------
DWORD Delete(void)
{
	namests_t ns;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get filename to search
	GetNameSts(GetReqAddr(14), &ns);

	// Call filesystem
	nResult = FS_Delete(&ns);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $46 - Get/set file attribute
// 
//	in	(offset	size)
//		12	1.B	Note that this becomes 0x01 when read from memory
//		13	1.B	attribute; read from memory when $FF
//		14	1.L	NAMESTS struct address
//
//---------------------------------------------------------------------------
DWORD Attribute(void)
{
	namests_t ns;
	DWORD attr;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get filename to search
	GetNameSts(GetReqAddr(14), &ns);

	// Target attribute
	attr = GetReqByte(13);

	// Call filesystem
	nResult = FS_Attribute(&ns, attr);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $47 - File search
// 
//	in	(offset	size)
//		 0	1.b	constant (26)
//		 1	1.b unit number
//		 2	1.b command ($47/$c7)
//		13	1.b	search attribute (Unused in WindrvXM. Written directly to search buffer.)
//		14	1.l	Filename buffer (namests format)
//		18	1.l	Search buffer (files format) Search data in progress and search results are written to this buffer.
//	out	(offset	size)
//		 3	1.b	error code (lower)
//		 4	1.b	''			(upper)
//		18	1.l result status
//
//  Search files from specified directory. Called from DOS _FILES.
//  When a search fails, or succeeds but not using wildcards,
//  we write -1 to the search buffer offset to make the next search fail.
//  When a file is found, we set its data as well as the following data
//  for the next search: sector number, offset, and in the case of the 
//  root directory, also the remaining sectors. The search drive, attribute,
//  and path name are set in the DOS call processing, so writing is not needed.
//
//	<NAMESTS struct>
//	(offset	size)
//	0	 1.b	NAMWLD	0: no wildcard -1:no specified file
//				(number of chars of wildcard)
//	1	 1.b	NAMDRV	drive number (A=0,B=1,...,Z=25)
//	2	65.b	NAMPTH	path ('\' + optional subdirectory + '\')
//	67	 8.b	NAMNM1	filename (first 8 chars)
//	75	 3.b	NAMEXT	file ending
//	78	10.b	NAMNM2	filename (remaining 10 chars)
//
//  Note that 0x09 (TAB) is used for path separator, rather than 0x2F (/) or 0x5C (\)
//
//---------------------------------------------------------------------------
DWORD Files(void)
{
	BYTE *files;
	files_t info;
	namests_t ns;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Memory for storing search in progress
	files = GetReqAddr(18);
	GetFiles(files, &info);

	// Get filename to search
	GetNameSts(GetReqAddr(14), &ns);

	// Call filesystem
	nResult = FS_Files((DWORD)files, &ns, &info);

	// Apply search results
	if (nResult >= 0) {
		SetFiles(files, &info);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $48 - Search next file
// 
//	in	(offset	size)
//		18	1.L	FILES struct address
//
//---------------------------------------------------------------------------
DWORD NFiles(void)
{
	BYTE *files;
	files_t info;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Read work memory
	files = GetReqAddr(18);
	GetFiles(files, &info);

	// Call filesystem
	nResult = FS_NFiles((DWORD)files, &info);

	// Apply search results
	if (nResult >= 0) {
		SetFiles(files, &info);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $49 - Create file (Create)
// 
//	in	(offset	size)
//		 1	1.B unit number
//		13	1.B	attribute
//		14	1.L	NAMESTS struct address
//		18	1.L	mode (0:_NEWFILE 1:_CREATE)
//		22	1.L	FCB struct address
//
//---------------------------------------------------------------------------
DWORD Create(void)
{
	namests_t ns;
	BYTE *pFcb;
	fcb_t fcb;
	DWORD attr;
	BOOL force;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get target filename
	GetNameSts(GetReqAddr(14), &ns);

	// Get FCB
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// Attribute
	attr = GetReqByte(13);

	// Forced overwrite mode
	force = (BOOL)GetReqLong(18);

	// Call filesystem
	nResult = FS_Create((DWORD)pFcb, &ns, &fcb, attr, force);

	// Apply results
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4A - File open
// 
//	in	(offset	size)
//		 1	1.B unit number
//		14	1.L	NAMESTS struct address
//		22	1.L	FCB struct address
//				Most parameters are already set in FCB.
//				Overwrite time and date at the moment of opening.
//				Overwrite if size is 0.
//
//---------------------------------------------------------------------------
DWORD Open(void)
{
	namests_t ns;
	BYTE *pFcb;
	fcb_t fcb;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get target filename
	GetNameSts(GetReqAddr(14), &ns);

	// Get FCB
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// Call filesystem
	nResult = FS_Open((DWORD)pFcb, &ns, &fcb);

	// Apply results
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4B - File close
// 
//	in	(offset	size)
//		22	1.L	FCB struct address
//
//---------------------------------------------------------------------------
DWORD Close(void)
{
	BYTE *pFcb;
	fcb_t fcb;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get FCB
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// Call filesystem
	nResult = FS_Close((DWORD)pFcb, &fcb);

	// Apply results
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4C - Read file
// 
//	in	(offset	size)
//		14	1.L	Reading buffer; Read the file contents here.
//		18	1.L	Size; It may be set to values exceeding 16MB
//		22	1.L	FCB struct address
//
//  No guarantee for behavior when passing address where bus error occurs.
// 
//  In the 20th century, a small number of apps used a trick where reading a
//  negative value meant "read all the files!" Wh, what? This file is over 16MB! 
// 
//  Back then, anything over 4~12MB was treated as "inifity" and often truncated
//  out of helpfulness, which is not needed in this day and age.
//  On the other hand, it might be a good idea to truncate at 12MB or 16MB.
//
//---------------------------------------------------------------------------
DWORD Read(void)
{
	BYTE *pFcb;
	fcb_t fcb;
	BYTE *pAddress;
	DWORD nSize;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get FCB
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// Read buffer
	pAddress = GetReqAddr(14);

	// Read size
	nSize = GetReqLong(18);

	// Clipping
	if (nSize >= WINDRV_CLIPSIZE_MAX) {
		nSize = WINDRV_CLIPSIZE_MAX;
	}

	// Call filesystem
	nResult = FS_Read((DWORD)pFcb, &fcb, pAddress, nSize);

	// Apply results
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4D - Write file
// 
//	in	(offset	size)
//		14	1.L	Write buffer; Write file contents here
//		18	1.L	Size; If a negative number, treated the same as specifying file size
//		22	1.L	FCB struct address
//
//  No guarantee for behavior when passing address where bus error occurs.
//
//---------------------------------------------------------------------------
DWORD Write(void)
{
	BYTE *pFcb;
	fcb_t fcb;
	BYTE *pAddress;
	DWORD nSize;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get FCB
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// Write buffer
	pAddress = GetReqAddr(14);

	// Write size
	nSize = GetReqLong(18);

	// Clipping
	if (nSize >= WINDRV_CLIPSIZE_MAX) {
		nSize = WINDRV_CLIPSIZE_MAX;
	}

	// Call filesystem
	nResult = FS_Write((DWORD)pFcb, &fcb, pAddress, nSize);

	// Apply results
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}
	
	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4E - File seek
// 
//	in	(offset	size)
//		12	1.B	This becomes 0x2B sometimes, and 0 other times
//		13	1.B	mode
//		18	1.L	offset
//		22	1.L	FCB struct address
//
//---------------------------------------------------------------------------
DWORD Seek(void)
{
	BYTE *pFcb;
	fcb_t fcb;
	DWORD nMode;
	DWORD nOffset;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get FCB
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// Seek mode
	nMode = GetReqByte(13);

	// Seek offset
	nOffset = GetReqLong(18);

	// Call filesystem
	nResult = FS_Seek((DWORD)pFcb, &fcb, nMode, nOffset);

	// Apply results
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4F - Get/set file timestamp
// 
//	in	(offset	size)
//		18	1.W	DATE
//		20	1.W	TIME
//		22	1.L	FCB struct address
//
//  Possible to change settings when FCB is opened in read mode too.
//  Note: Cannot make the judgement of read-only with only FCB
//
//---------------------------------------------------------------------------
DWORD TimeStamp(void)
{
	BYTE *pFcb;
	fcb_t fcb;
	DWORD nTime;
	DWORD nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get FCB
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// Get timestamp
	nTime = GetReqLong(18);

	// Call filesystem
	nResult = FS_TimeStamp((DWORD)pFcb, &fcb, nTime);

	// Apply results
	if (nResult < 0xFFFF0000) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $50 - Get capacity
// 
//	in	(offset	size)
//		 0	1.b	constant (26)
//		 1	1.b unit number
//		 2	1.b	command ($50/$d0)
//		14	1.l	buffer address
//	out	(offset	size)
//		 3	1.b	error code (lower)
//		 4	1.b	''			(upper)
//		18	1.l result status
//
//  Get the total and available media capacity, as well as cluster / sector size.
//  The contents to write to buffer follows. Returns number of bytes that can be
//  used for result status.
//
//	(offset	size)
//	0	1.w	available number of clusters
//	2	1.w	total number of clusters
//	4	1.w	number of sectors per 1 cluster
//	6	1.w	number of bytes per 1 sector
//
//---------------------------------------------------------------------------
DWORD GetCapacity(void)
{
	BYTE *pCapacity;
	capacity_t cap;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get buffer
	pCapacity = GetReqAddr(14);

#if 0
	// Call filesystem
	nResult = FS_GetCapacity(&cap);
#else
	// Try skipping since the contents are always returned
	cap.freearea = 0xFFFF;
	cap.clusters = 0xFFFF;
	cap.sectors = 64;
	cap.bytes = 512;
	nResult = 0x7FFF8000;
#endif

	// Apply results
	if (nResult >= 0) {
		SetCapacity(pCapacity, &cap);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $51 - Inspect/control drive status
// 
//	in	(offset	size)
//		 1	1.B unit number
//		13	1.B	status	0: status inspection 1: eject
//
//---------------------------------------------------------------------------
DWORD CtrlDrive(void)
{
	ctrldrive_t ctrl;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get drive status
	ctrl.status = GetReqByte(13);

	// Call filesystem
	nResult = FS_CtrlDrive(&ctrl);

	// Apply results
	if (nResult >= 0) {
		SetReqByte(13, ctrl.status);
	}
	
	return nResult;
}

//---------------------------------------------------------------------------
//
//  $52 - Get DPB
// 
//	in	(offset	size)
//		 0	1.b	constant (26)
//		 1	1.b unit number
//		 2	1.b	command ($52/$d2)
//		14	1.l	buffer address (points at starting address + 2)
//	out	(offset	size)
//		 3	1.b	error code (lower)
//		 4	1.b	''			(upper)
//		18	1.l result status
//
//  Specified media data is returned as DPB v1. Data needed to be set
//  for this command are as follows (parantheses indicate what's set by DOS calls.)
//  Note that the buffer adress is what's pointed at by offset 2.
//
//	(offset	size)
//	 0	1.b	(drive number)
//	 1	1.b	(unit number)
//	 2	1.w	number of bytes per 1 sector
//	 4	1.b	number of sectors - 1 per 1 cluster
//	 5	1.b	Number of cluster -> sector 
//			bit 7 = 1 in MS-DOS format FAT (16bit Intel array)
//	 6	1.w	FAT first sector number
//	 8	1.b	Number of FAT allocations
//	 9	1.b	Number of FAT controlled sectors (excluding duplicates)
//	10	1.w	Number of files in the root directory
//	12	1.w	First sector number of data memory
//	14	1.w	Total number of clusters + 1
//	16	1.w	First sector number of root directory
//	18	1.l	(Driver head address)
//	22	1.b	(Physical drive name in lower-case)
//	23	1.b	(DPB usage flag: normally 0)
//	24	1.l	(Next DPB address)
//	28	1.w	(Cluster number of current directory: normally 0)
//	30	64.b	(Current directory name)
//
//---------------------------------------------------------------------------
DWORD GetDPB(void)
{
	BYTE *pDpb;
	dpb_t dpb;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get DPB
	pDpb = GetReqAddr(14);

	// Call filesystem
	nResult = FS_GetDPB(&dpb);

	// Apply results
	if (nResult >= 0) {
		SetDpb(pDpb, &dpb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $53 - Read sector
// 
//	in	(offset	size)
//		 1	1.B unit number
//		14	1.L buffer address
//		18	1.L number of sectors
//		22	1.L sector number
//
//---------------------------------------------------------------------------
DWORD DiskRead(void)
{
	BYTE *pAddress;
	DWORD nSize;
	DWORD nSector;
	BYTE buffer[0x200];
	int nResult;
	int i;

	ASSERT(this);
	ASSERT(fs);

	pAddress = GetReqAddr(14);	// Address (upper bit is extension flag)
	nSize = GetReqLong(18);		// Number of sectors
	nSector = GetReqLong(22);	// Sector number

	// Call filesystem
	nResult = FS_DiskRead(buffer, nSector, nSize);

	// Apply results
	if (nResult >= 0) {
		for (i = 0; i < sizeof(buffer); i++) {
			*pAddress = buffer[i];
			pAddress++;
		}
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $54 - Write sector
// 
//	in	(offset	size)
//		 1	1.B unit number
//		14	1.L buffer address
//		18	1.L number of sectors
//		22	1.L sector number
//
//---------------------------------------------------------------------------
DWORD DiskWrite(void)
{
	BYTE *pAddress;
	DWORD nSize;
	DWORD nSector;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	pAddress = GetReqAddr(14);	// Address (upper bit is extension flag)
	nSize = GetReqLong(18);		// Number of sectors
	nSector = GetReqLong(22);	// Sector number

	// Call filesystem
	nResult = FS_DiskWrite();

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $55 - IOCTRL
// 
//	in	(offset	size)
//		 1	1.B unit number
//		14	1.L	parameter
//		18	1.W	feature number
//
//---------------------------------------------------------------------------
DWORD Ioctrl(void)
{
	DWORD param;
	DWORD func;
	ioctrl_t ioctrl;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Get IOCTRL
	param = GetReqLong(14);		// Parameter
	func = GetReqWord(18);		// Feature number
	GetIoctrl(param, func, &ioctrl);

	// Call filesystem
	nResult = FS_Ioctrl(func, &ioctrl);

	// Apply results
	if (nResult >= 0)
		SetIoctrl(param, func, &ioctrl);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $56 - Flush
// 
//	in	(offset	size)
//		 1	1.b unit number
//
//---------------------------------------------------------------------------
DWORD Flush(void)
{
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Call filesystem
	nResult = FS_Flush();

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $57 - Media change check
// 
//	in	(offset	size)
//		 0	1.b	constant (26)
//		 1	1.b unit number
//		 2	1.b	Command ($57/$d7)
//	out	(offset	size)
//		 3	1.b	error code (lower)
//		 4	1.b	''			(upper)
//		18	1.l result status
//
//  Checks if the media has been changed or not. Format if it has changed.
//  The verification is done within this command.
//
//---------------------------------------------------------------------------
DWORD CheckMedia(void)
{
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Call filesystem
	nResult = FS_CheckMedia();

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $58 - Lock
// 
//	in	(offset	size)
//		 1	1.b unit number
//
//---------------------------------------------------------------------------
DWORD Lock(void)
{
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// Call filesystem
	nResult = FS_Lock();

	return nResult;
}

//---------------------------------------------------------------------------
//
//  Execute command
//
//---------------------------------------------------------------------------
DWORD ExecuteCommand()
{
	ASSERT(this);

	// Clear error data
	SetReqByte(3, 0);
	SetReqByte(4, 0);

	// Command number
	command = (DWORD)GetReqByte(2);	// Bit 7 is verify flag

	// Unit number
	unit = GetReqByte(1);

	// Command branching
	switch (command & 0x7F) {
		case 0x40: return InitDevice();	// $40 - Device startup
		case 0x41: return CheckDir();	// $41 - Directory check
		case 0x42: return MakeDir();	// $42 - Create directory
		case 0x43: return RemoveDir();	// $43 - Delete directory
		case 0x44: return Rename();		// $44 - Change file name
		case 0x45: return Delete();		// $45 - Delete file
		case 0x46: return Attribute();	// $46 - Get / set file attribute
		case 0x47: return Files();		// $47 - Find file
		case 0x48: return NFiles();		// $48 - Find next file
		case 0x49: return Create();		// $49 - Create file
		case 0x4A: return Open();		// $4A - Open file
		case 0x4B: return Close();		// $4B - Close file
		case 0x4C: return Read();		// $4C - Read file
		case 0x4D: return Write();		// $4D - Write file
		case 0x4E: return Seek();		// $4E - Seek file
		case 0x4F: return TimeStamp();	// $4F - Get / set file timestamp
		case 0x50: return GetCapacity();// $50 - Get capacity
		case 0x51: return CtrlDrive();	// $51 - Inspect / control drive status
		case 0x52: return GetDPB();		// $52 - Get DPB
		case 0x53: return DiskRead();	// $53 - Read sectors
		case 0x54: return DiskWrite();	// $54 - Write sectors
		case 0x55: return Ioctrl();		// $55 - IOCTRL
		case 0x56: return Flush();		// $56 - Flush
		case 0x57: return CheckMedia();	// $57 - Media change check
		case 0x58: return Lock();		// $58 - Lock
	}

	return FS_FATAL_INVALIDCOMMAND;
}

//---------------------------------------------------------------------------
//
//  Initialization
//
//---------------------------------------------------------------------------
BOOL Init()
{
	ASSERT(this);

	return SCSI_Init();
}

//---------------------------------------------------------------------------
//
//  Execution
//
//---------------------------------------------------------------------------
void Process(DWORD nA5)
{
	ASSERT(this);
	ASSERT(nA5 <= 0xFFFFFF);
	ASSERT(m_bAlloc);
	ASSERT(m_bFree);

	// Request header address
	request = (BYTE*)nA5;

	// Command execution
	SetResult(ExecuteCommand());
}
