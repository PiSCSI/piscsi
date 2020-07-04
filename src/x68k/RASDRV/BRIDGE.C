//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technorogy.
//  Copyright (C) 2016-2017 GIMONS
//	[ ホストファイルシステム ブリッジドライバ ]
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
//  変数宣言
//
//---------------------------------------------------------------------------
volatile BYTE *request;				// リクエストヘッダアドレス
DWORD command;						// コマンド番号
DWORD unit;							// ユニット番号

//===========================================================================
//
/// ファイルシステム(SCSI連携)
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
// 初期化
//
//---------------------------------------------------------------------------
BOOL SCSI_Init(void)
{
	int i;
	INQUIRY_T inq;

	// SCSI ID未定
	scsiid = -1;
	
	for (i = 0; i <= 7; i++) {
		if (S_INQUIRY(sizeof(INQUIRY_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}

		if (memcmp(&(inq.ProductID), "RASCSI BRIDGE", 13) != 0) {
			continue;
		}

		// SCSI ID確定
		scsiid = i;
		return TRUE;
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
// コマンド送信
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

	// セレクトがタイムアウトする時には再試行
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

	// セレクトがタイムアウトする時には再試行
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
// コマンド呼び出し
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
	
	// セレクトがタイムアウトする時には再試行
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

	// セレクトがタイムアウトする時には再試行
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

	// エラーなら返却データの受信を止める
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

	// セレクトがタイムアウトする時には再試行
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
// オプションデータ取得
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

	// セレクトがタイムアウトする時には再試行
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
// オプションデータ書き込み
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

	// セレクトがタイムアウトする時には再試行
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
/// ファイルシステム
//
//===========================================================================

//---------------------------------------------------------------------------
//
//  $40 - デバイス起動
//
//---------------------------------------------------------------------------
DWORD FS_InitDevice(const argument_t* pArgument)
{
	return (DWORD)SCSI_SendCmd((BYTE*)pArgument, sizeof(argument_t));
}

//---------------------------------------------------------------------------
//
//  $41 - ディレクトリチェック
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
//  $42 - ディレクトリ作成
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
//  $43 - ディレクトリ削除
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
//  $44 - ファイル名変更
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
//  $45 - ファイル削除
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
//  $46 - ファイル属性取得/設定
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
//  $47 - ファイル検索
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
//  $48 - ファイル次検索
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
//  $49 - ファイル作成
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
//  $4A - ファイルオープン
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
//  $4B - ファイルクローズ
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
//  $4C - ファイル読み込み
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
//  $4D - ファイル書き込み
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
//  $4E - ファイルシーク
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
//  $4F - ファイル時刻取得/設定
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
//  $50 - 容量取得
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
//  $51 - ドライブ状態検査/制御
//
//---------------------------------------------------------------------------
int FS_CtrlDrive(ctrldrive_t* pCtrlDrive)
{
#if 1
	// 負荷が高いのでここで暫定処理
	switch (pCtrlDrive->status) {
		case 0:		// 状態検査
		case 9:		// 状態検査2
			pCtrlDrive->status = 0x42;
			return pCtrlDrive->status;
		case 1:		// イジェクト
		case 2:		// イジェクト禁止1 (未実装)
		case 3:		// イジェクト許可1 (未実装)
		case 4:		// メディア未挿入時にLED点滅 (未実装)
		case 5:		// メディア未挿入時にLED消灯 (未実装)
		case 6:		// イジェクト禁止2 (未実装)
		case 7:		// イジェクト許可2 (未実装)
			return 0;

		case 8:		// イジェクト検査
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
//  $52 - DPB取得
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
//  $53 - セクタ読み込み
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
//  $54 - セクタ書き込み
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
//  $56 - フラッシュ
//
//---------------------------------------------------------------------------
int FS_Flush()
{
#if 1
	// 未サポート
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
//  $57 - メディア交換チェック
//
//---------------------------------------------------------------------------
int FS_CheckMedia()
{
#if 1
	// 負荷が高いので暫定処理
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
//  $58 - 排他制御
//
//---------------------------------------------------------------------------
int FS_Lock()
{
#if 1
	// 未サポート
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
//	コマンドハンドラ
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
//  NAMESTS読み込み
//
//---------------------------------------------------------------------------
void GetNameStsPath(BYTE *addr, namests_t* pNamests)
{
	DWORD i;

	ASSERT(this);
	ASSERT(pNamests);

	// ワイルドカード情報
	pNamests->wildcard = *addr;

	// ドライブ番号
	pNamests->drive = addr[1];

	// パス名
	for (i = 0; i < sizeof(pNamests->path); i++) {
		pNamests->path[i] = addr[2 + i];
	}

	// ファイル名1
	memset(pNamests->name, 0x20, sizeof(pNamests->name));

	// 拡張子
	memset(pNamests->ext, 0x20, sizeof(pNamests->ext));

	// ファイル名2
	memset(pNamests->add, 0, sizeof(pNamests->add));
}

//---------------------------------------------------------------------------
//
//  NAMESTS読み込み
//
//---------------------------------------------------------------------------
void GetNameSts(BYTE *addr, namests_t* pNamests)
{
	DWORD i;

	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(addr <= 0xFFFFFF);

	// ワイルドカード情報
	pNamests->wildcard = *addr;

	// ドライブ番号
	pNamests->drive = addr[1];

	// パス名
	for (i = 0; i < sizeof(pNamests->path); i++) {
		pNamests->path[i] = addr[2 + i];
	}

	// ファイル名1
	for (i = 0; i < sizeof(pNamests->name); i++) {
		pNamests->name[i] = addr[67 + i];
	}

	// 拡張子
	for (i = 0; i < sizeof(pNamests->ext); i++) {
		pNamests->ext[i] = addr[75 + i];
	}

	// ファイル名2
	for (i = 0; i < sizeof(pNamests->add); i++) {
		pNamests->add[i] = addr[78 + i];
	}
}

//---------------------------------------------------------------------------
//
//  FILES読み込み
//
//---------------------------------------------------------------------------
void GetFiles(BYTE *addr, files_t* pFiles)
{
	ASSERT(this);
	ASSERT(pFiles);
	ASSERT(addr <= 0xFFFFFF);

	// 検索情報
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
//  FILES書き込み
//
//---------------------------------------------------------------------------
void SetFiles(BYTE *addr, const files_t* pFiles)
{
	DWORD i;

	ASSERT(this);
	ASSERT(pFiles);

	*((DWORD*)&addr[2]) = pFiles->sector;
	*((WORD*)&addr[8]) = pFiles->offset;

	// ファイル情報
	addr[21] = pFiles->attr;
	*((WORD*)&addr[22]) = pFiles->time;
	*((WORD*)&addr[24]) = pFiles->date;
	*((DWORD*)&addr[26]) = pFiles->size;

	// フルファイル名
	addr += 30;
	for (i = 0; i < sizeof(pFiles->full); i++) {
		*addr = pFiles->full[i];
		addr++;
	}
}

//---------------------------------------------------------------------------
//
//  FCB読み込み
//
//---------------------------------------------------------------------------
void GetFcb(BYTE *addr, fcb_t* pFcb)
{
	ASSERT(this);
	ASSERT(pFcb);

	// FCB情報
	pFcb->fileptr = *((DWORD*)&addr[6]);
	pFcb->mode = *((WORD*)&addr[14]);

	// 属性
	pFcb->attr = addr[47];

	// FCB情報
	pFcb->time = *((WORD*)&addr[58]);
	pFcb->date = *((WORD*)&addr[60]);
	pFcb->size = *((DWORD*)&addr[64]);
}

//---------------------------------------------------------------------------
//
//  FCB書き込み
//
//---------------------------------------------------------------------------
void SetFcb(BYTE *addr, const fcb_t* pFcb)
{
	ASSERT(this);
	ASSERT(pFcb);

	// FCB情報
	*((DWORD*)&addr[6]) = pFcb->fileptr;
	*((WORD*)&addr[14]) = pFcb->mode;

	// 属性
	addr[47] = pFcb->attr;

	// FCB情報
	*((WORD*)&addr[58]) = pFcb->time;
	*((WORD*)&addr[60]) = pFcb->date;
	*((DWORD*)&addr[64]) = pFcb->size;
}

//---------------------------------------------------------------------------
//
//  CAPACITY書き込み
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
//  DPB書き込み
//
//---------------------------------------------------------------------------
void SetDpb(BYTE *addr, const dpb_t* pDpb)
{
	ASSERT(this);
	ASSERT(pDpb);

	// DPB情報
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
//  IOCTRL読み込み
//
//---------------------------------------------------------------------------
void GetIoctrl(DWORD param, DWORD func, ioctrl_t* pIoctrl)
{
	DWORD *lp;
	
	ASSERT(this);
	ASSERT(pIoctrl);

	switch (func) {
		case 2:
			// メディア再認識
			pIoctrl->param = param;
			return;

		case -2:
			// オプション設定
			lp = (DWORD*)param;
			pIoctrl->param = *lp;
			return;
	}
}

//---------------------------------------------------------------------------
//
//  IOCTRL書き込み
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
			// メディアIDの獲得
			wp = (WORD*)param;
			*wp = pIoctrl->media;
			return;

		case 1:
			// Human68k互換のためのダミー
			lp = (DWORD*)param;
			*lp = pIoctrl->param;
			return;

		case -1:
			// 常駐判定
			bp = (BYTE*)param;
			for (i = 0; i < 8; i++) {
				*bp = pIoctrl->buffer[i];
				bp++;
			}
			return;

		case -3:
			// オプション獲得
			lp = (DWORD*)param;
			*lp = pIoctrl->param;
			return;
	}
}

//---------------------------------------------------------------------------
//
//  ARGUMENT読み込み
// 
//  バッファサイズよりも長い場合は転送を打ち切って必ず終端する。
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
//  終了値書き込み
//
//---------------------------------------------------------------------------
void SetResult(DWORD nResult)
{
	DWORD code;

	ASSERT(this);

	// 致命的エラー判定
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
			//  @note リトライ可能を返すときは、(a5 + 18)を書き換えてはいけない。
			//  その後白帯で Retry を選択した場合、書き換えた値を読み込んで誤動作してしまう。
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
//  $40 - デバイス起動
// 
//	in	(offset	size)
//		 0	1.b	定数(22)
//		 2	1.b	コマンド($40/$c0)
//		18	1.l	パラメータアドレス
//		22	1.b	ドライブ番号
//	out	(offset	size)
//		 3	1.b	エラーコード(下位)
//		 4	1.b	〃			(上位)
//		13	1.b	ユニット数
//		14	1.l	デバイスドライバの終了アドレス + 1
//
//    ローカルドライブのコマンド 0 と同様に組み込み時に呼ばれるが、BPB 及
//  びそのポインタの配列を用意する必要はない.
//  他のコマンドと違い、このコマンドだけa5 + 1には有効な値が入っていない
//  (0初期化なども期待してはいけない)ので注意すること。
//
//---------------------------------------------------------------------------
DWORD InitDevice(void)
{
	argument_t arg;
	DWORD units;

	ASSERT(this);
	ASSERT(fs);

	// オプション内容を獲得
	GetArgument(GetReqAddr(18), &arg);

	// Human68k側で利用可能なドライブ数の範囲で、ファイルシステムを構築
	units = FS_InitDevice(&arg);

	// ドライブ数を返信
	SetReqByte(13, units);

	return 0;
}

//---------------------------------------------------------------------------
//
//  $41 - ディレクトリチェック
// 
//	in	(offset	size)
//		14	1.L	NAMESTS構造体アドレス
//
//---------------------------------------------------------------------------
DWORD CheckDir(void)
{
	namests_t ns;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// 検索対象ファイル名獲得
	GetNameStsPath(GetReqAddr(14), &ns);

	// ファイルシステム呼び出し
	nResult = FS_CheckDir(&ns);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $42 - ディレクトリ作成
// 
//	in	(offset	size)
//		14	1.L	NAMESTS構造体アドレス
//
//---------------------------------------------------------------------------
DWORD MakeDir(void)
{
	namests_t ns;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// 検索対象ファイル名獲得
	GetNameSts(GetReqAddr(14), &ns);

	// ファイルシステム呼び出し
	nResult = FS_MakeDir(&ns);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $43 - ディレクトリ削除
// 
//	in	(offset	size)
//		14	1.L	NAMESTS構造体アドレス
//
//---------------------------------------------------------------------------
DWORD RemoveDir(void)
{
	namests_t ns;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// 検索対象ファイル名獲得
	GetNameSts(GetReqAddr(14), &ns);

	// ファイルシステム呼び出し
	nResult = FS_RemoveDir(&ns);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $44 - ファイル名変更
// 
//	in	(offset	size)
//		14	1.L	NAMESTS構造体アドレス 旧ファイル名
//		18	1.L	NAMESTS構造体アドレス 新ファイル名
//
//---------------------------------------------------------------------------
DWORD Rename(void)
{
	namests_t ns;
	namests_t ns_new;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// 検索対象ファイル名獲得
	GetNameSts(GetReqAddr(14), &ns);
	GetNameSts(GetReqAddr(18), &ns_new);

	// ファイルシステム呼び出し
	nResult = FS_Rename(&ns, &ns_new);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $45 - ファイル削除
// 
//	in	(offset	size)
//		14	1.L	NAMESTS構造体アドレス
//
//---------------------------------------------------------------------------
DWORD Delete(void)
{
	namests_t ns;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// 検索対象ファイル名獲得
	GetNameSts(GetReqAddr(14), &ns);

	// ファイルシステム呼び出し
	nResult = FS_Delete(&ns);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $46 - ファイル属性取得/設定
// 
//	in	(offset	size)
//		12	1.B	読み出し時に0x01になるので注意
//		13	1.B	属性 $FFだと読み出し
//		14	1.L	NAMESTS構造体アドレス
//
//---------------------------------------------------------------------------
DWORD Attribute(void)
{
	namests_t ns;
	DWORD attr;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// 検索対象ファイル名獲得
	GetNameSts(GetReqAddr(14), &ns);

	// 対象属性
	attr = GetReqByte(13);

	// ファイルシステム呼び出し
	nResult = FS_Attribute(&ns, attr);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $47 - ファイル検索
// 
//	in	(offset	size)
//		 0	1.b	定数(26)
//		 1	1.b	ユニット番号
//		 2	1.b	コマンド($47/$c7)
//		13	1.b	検索属性 (WindrvXMでは未使用。検索バッファに値が書かれている)
//		14	1.l	ファイル名バッファ(namests 形式)
//		18	1.l	検索バッファ(files 形式) このバッファに検索途中情報と検索結果を書き込む
//	out	(offset	size)
//		 3	1.b	エラーコード(下位)
//		 4	1.b	〃			(上位)
//		18	1.l	リザルトステータス
//
//    ディレクトリから指定ファイルを検索する. DOS _FILES から呼び出される.
//    検索に失敗した場合、若しくは検索に成功してもワイルドカードが使われて
//  いない場合は、次回検索時に必ず失敗させる為に検索バッファのオフセットに
//  -1 を書き込む. 検索が成功した場合は見つかったファイルの情報を設定する
//  と共に、次検索用の情報のセクタ番号、オフセット、ルートディレクトリの場
//  合は更に残りセクタ数を設定する. 検索ドライブ・属性、パス名は DOS コー
//  ル処理内で設定されるので書き込む必要はない.
//
//	<NAMESTS構造体>
//	(offset	size)
//	0	 1.b	NAMWLD	0:ワイルドカードなし -1:ファイル指定なし
//				(ワイルドカードの文字数)
//	1	 1.b	NAMDRV	ドライブ番号(A=0,B=1,…,Z=25)
//	2	65.b	NAMPTH	パス('\'＋あればサブディレクトリ名＋'\')
//	67	 8.b	NAMNM1	ファイル名(先頭 8 文字)
//	75	 3.b	NAMEXT	拡張子
//	78	10.b	NAMNM2	ファイル名(残りの 10 文字)
//
//  パス区切り文字は0x2F(/)や0x5C(\)ではなく0x09(TAB)を使っているので注意。
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

	// 検索途中経過格納領域
	files = GetReqAddr(18);
	GetFiles(files, &info);

	// 検索対象ファイル名獲得
	GetNameSts(GetReqAddr(14), &ns);

	// ファイルシステム呼び出し
	nResult = FS_Files((DWORD)files, &ns, &info);

	// 検索結果の反映
	if (nResult >= 0) {
		SetFiles(files, &info);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $48 - ファイル次検索
// 
//	in	(offset	size)
//		18	1.L	FILES構造体アドレス
//
//---------------------------------------------------------------------------
DWORD NFiles(void)
{
	BYTE *files;
	files_t info;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// ワーク領域の読み込み
	files = GetReqAddr(18);
	GetFiles(files, &info);

	// ファイルシステム呼び出し
	nResult = FS_NFiles((DWORD)files, &info);

	// 検索結果の反映
	if (nResult >= 0) {
		SetFiles(files, &info);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $49 - ファイル作成(Create)
// 
//	in	(offset	size)
//		 1	1.B	ユニット番号
//		13	1.B	属性
//		14	1.L	NAMESTS構造体アドレス
//		18	1.L	モード (0:_NEWFILE 1:_CREATE)
//		22	1.L	FCB構造体アドレス
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

	// 対象ファイル名獲得
	GetNameSts(GetReqAddr(14), &ns);

	// FCB獲得
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// 属性
	attr = GetReqByte(13);

	// 強制上書きモード
	force = (BOOL)GetReqLong(18);

	// ファイルシステム呼び出し
	nResult = FS_Create((DWORD)pFcb, &ns, &fcb, attr, force);

	// 結果の反映
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4A - ファイルオープン
// 
//	in	(offset	size)
//		 1	1.B	ユニット番号
//		14	1.L	NAMESTS構造体アドレス
//		22	1.L	FCB構造体アドレス
//				既にFCBにはほとんどのパラメータが設定済み
//				時刻・日付はオープンした瞬間のものになってるので上書き
//				サイズは0になっているので上書き
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

	// 対象ファイル名獲得
	GetNameSts(GetReqAddr(14), &ns);

	// FCB獲得
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// ファイルシステム呼び出し
	nResult = FS_Open((DWORD)pFcb, &ns, &fcb);

	// 結果の反映
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4B - ファイルクローズ
// 
//	in	(offset	size)
//		22	1.L	FCB構造体アドレス
//
//---------------------------------------------------------------------------
DWORD Close(void)
{
	BYTE *pFcb;
	fcb_t fcb;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// FCB獲得
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// ファイルシステム呼び出し
	nResult = FS_Close((DWORD)pFcb, &fcb);

	// 結果の反映
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4C - ファイル読み込み
// 
//	in	(offset	size)
//		14	1.L	読み込みバッファ ここにファイル内容を読み込む
//		18	1.L	サイズ 16MBを超える値が指定される可能性もあり
//		22	1.L	FCB構造体アドレス
//
//  バスエラーが発生するアドレスを指定した場合の動作は保証しない。
// 
//  20世紀のアプリは「負の数だとファイルを全部読む」という作法で書かれている
//  可能性が微レ存。あれれー？このファイル16MB超えてるよー？(CV.高山みなみ)
// 
//  むしろ4～12MBくらいの当時のプログラマーから見た実質的な「∞」の値で
//  クリップするような配慮こそが現代では必要なのではなかろうか。
//  または、末尾が12MBと16MB位置を超えたらサイズをクリップすると良いかも。
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

	// FCB獲得
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// 読み込みバッファ
	pAddress = GetReqAddr(14);

	// 読み込みサイズ
	nSize = GetReqLong(18);

	// クリッピング
	if (nSize >= WINDRV_CLIPSIZE_MAX) {
		nSize = WINDRV_CLIPSIZE_MAX;
	}

	// ファイルシステム呼び出し
	nResult = FS_Read((DWORD)pFcb, &fcb, pAddress, nSize);

	// 結果の反映
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4D - ファイル書き込み
// 
//	in	(offset	size)
//		14	1.L	書き込みバッファ ここにファイル内容を書き込む
//		18	1.L	サイズ 負の数ならファイルサイズを指定したのと同じ
//		22	1.L	FCB構造体アドレス
//
//  バスエラーが発生するアドレスを指定した場合の動作は保証しない。
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

	// FCB獲得
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// 書き込みバッファ
	pAddress = GetReqAddr(14);

	// 書き込みサイズ
	nSize = GetReqLong(18);

	// クリッピング
	if (nSize >= WINDRV_CLIPSIZE_MAX) {
		nSize = WINDRV_CLIPSIZE_MAX;
	}

	// ファイルシステム呼び出し
	nResult = FS_Write((DWORD)pFcb, &fcb, pAddress, nSize);

	// 結果の反映
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}
	
	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4E - ファイルシーク
// 
//	in	(offset	size)
//		12	1.B	0x2B になってるときがある 0のときもある
//		13	1.B	モード
//		18	1.L	オフセット
//		22	1.L	FCB構造体アドレス
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

	// FCB獲得
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// シークモード
	nMode = GetReqByte(13);

	// シークオフセット
	nOffset = GetReqLong(18);

	// ファイルシステム呼び出し
	nResult = FS_Seek((DWORD)pFcb, &fcb, nMode, nOffset);

	// 結果の反映
	if (nResult >= 0) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $4F - ファイル時刻取得/設定
// 
//	in	(offset	size)
//		18	1.W	DATE
//		20	1.W	TIME
//		22	1.L	FCB構造体アドレス
//
//  FCBが読み込みモードで開かれた状態でも設定変更が可能。
//  FCBだけでは書き込み禁止の判定ができないので注意。
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

	// FCB獲得
	pFcb = GetReqAddr(22);
	GetFcb(pFcb, &fcb);

	// 時刻獲得
	nTime = GetReqLong(18);

	// ファイルシステム呼び出し
	nResult = FS_TimeStamp((DWORD)pFcb, &fcb, nTime);

	// 結果の反映
	if (nResult < 0xFFFF0000) {
		SetFcb(pFcb, &fcb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $50 - 容量取得
// 
//	in	(offset	size)
//		 0	1.b	定数(26)
//		 1	1.b	ユニット番号
//		 2	1.b	コマンド($50/$d0)
//		14	1.l	バッファアドレス
//	out	(offset	size)
//		 3	1.b	エラーコード(下位)
//		 4	1.b	〃			(上位)
//		18	1.l	リザルトステータス
//
//    メディアの総容量/空き容量、クラスタ/セクタサイズを収得する. バッファ
//  に書き込む内容は以下の通り. リザルトステータスとして使用可能なバイト数
//  を返すこと.
//
//	(offset	size)
//	0	1.w	使用可能なクラスタ数
//	2	1.w	総クラスタ数
//	4	1.w	1 クラスタ当りのセクタ数
//	6	1.w	1 セクタ当りのバイト数
//
//---------------------------------------------------------------------------
DWORD GetCapacity(void)
{
	BYTE *pCapacity;
	capacity_t cap;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// バッファ取得
	pCapacity = GetReqAddr(14);

#if 0
	// ファイルシステム呼び出し
	nResult = FS_GetCapacity(&cap);
#else
	// いつも同じ内容が返ってくるのでスキップしてみる
	cap.freearea = 0xFFFF;
	cap.clusters = 0xFFFF;
	cap.sectors = 64;
	cap.bytes = 512;
	nResult = 0x7FFF8000;
#endif

	// 結果の反映
	if (nResult >= 0) {
		SetCapacity(pCapacity, &cap);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $51 - ドライブ状態検査/制御
// 
//	in	(offset	size)
//		 1	1.B	ユニット番号
//		13	1.B	状態	0: 状態検査 1: イジェクト
//
//---------------------------------------------------------------------------
DWORD CtrlDrive(void)
{
	ctrldrive_t ctrl;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// ドライブ状態取得
	ctrl.status = GetReqByte(13);

	// ファイルシステム呼び出し
	nResult = FS_CtrlDrive(&ctrl);

	// 結果の反映
	if (nResult >= 0) {
		SetReqByte(13, ctrl.status);
	}
	
	return nResult;
}

//---------------------------------------------------------------------------
//
//  $52 - DPB取得
// 
//	in	(offset	size)
//		 0	1.b	定数(26)
//		 1	1.b	ユニット番号
//		 2	1.b	コマンド($52/$d2)
//		14	1.l	バッファアドレス(先頭アドレス + 2 を指す)
//	out	(offset	size)
//		 3	1.b	エラーコード(下位)
//		 4	1.b	〃			(上位)
//		18	1.l	リザルトステータス
//
//    指定メディアの情報を v1 形式 DPB で返す. このコマンドで設定する必要
//  がある情報は以下の通り(括弧内は DOS コールが設定する). ただし、バッフ
//  ァアドレスはオフセット 2 を指したアドレスが渡されるので注意すること.
//
//	(offset	size)
//	 0	1.b	(ドライブ番号)
//	 1	1.b	(ユニット番号)
//	 2	1.w	1 セクタ当りのバイト数
//	 4	1.b	1 クラスタ当りのセクタ数 - 1
//	 5	1.b	クラスタ→セクタのシフト数
//			bit 7 = 1 で MS-DOS 形式 FAT(16bit Intel 配列)
//	 6	1.w	FAT の先頭セクタ番号
//	 8	1.b	FAT 領域の個数
//	 9	1.b	FAT の占めるセクタ数(複写分を除く)
//	10	1.w	ルートディレクトリに入るファイルの個数
//	12	1.w	データ領域の先頭セクタ番号
//	14	1.w	総クラスタ数 + 1
//	16	1.w	ルートディレクトリの先頭セクタ番号
//	18	1.l	(ドライバヘッダのアドレス)
//	22	1.b	(小文字の物理ドライブ名)
//	23	1.b	(DPB 使用フラグ:常に 0)
//	24	1.l	(次の DPB のアドレス)
//	28	1.w	(カレントディレクトリのクラスタ番号:常に 0)
//	30	64.b	(カレントディレクトリ名)
//
//---------------------------------------------------------------------------
DWORD GetDPB(void)
{
	BYTE *pDpb;
	dpb_t dpb;
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// DPB取得
	pDpb = GetReqAddr(14);

	// ファイルシステム呼び出し
	nResult = FS_GetDPB(&dpb);

	// 結果の反映
	if (nResult >= 0) {
		SetDpb(pDpb, &dpb);
	}

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $53 - セクタ読み込み
// 
//	in	(offset	size)
//		 1	1.B	ユニット番号
//		14	1.L	バッファアドレス
//		18	1.L	セクタ数
//		22	1.L	セクタ番号
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

	pAddress = GetReqAddr(14);	// アドレス (上位ビットが拡張フラグ)
	nSize = GetReqLong(18);		// セクタ数
	nSector = GetReqLong(22);	// セクタ番号

	// ファイルシステム呼び出し
	nResult = FS_DiskRead(buffer, nSector, nSize);

	// 結果の反映
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
//  $54 - セクタ書き込み
// 
//	in	(offset	size)
//		 1	1.B	ユニット番号
//		14	1.L	バッファアドレス
//		18	1.L	セクタ数
//		22	1.L	セクタ番号
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

	pAddress = GetReqAddr(14);	// アドレス(上位ビットが拡張フラグ)
	nSize = GetReqLong(18);		// セクタ数
	nSector = GetReqLong(22);	// セクタ番号

	// ファイルシステム呼び出し
	nResult = FS_DiskWrite();

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $55 - IOCTRL
// 
//	in	(offset	size)
//		 1	1.B	ユニット番号
//		14	1.L	パラメータ
//		18	1.W	機能番号
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

	// IOCTRL取得
	param = GetReqLong(14);		// パラメータ
	func = GetReqWord(18);		// 機能番号
	GetIoctrl(param, func, &ioctrl);

	// ファイルシステム呼び出し
	nResult = FS_Ioctrl(func, &ioctrl);

	// 結果の反映
	if (nResult >= 0)
		SetIoctrl(param, func, &ioctrl);

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $56 - フラッシュ
// 
//	in	(offset	size)
//		 1	1.B	ユニット番号
//
//---------------------------------------------------------------------------
DWORD Flush(void)
{
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// ファイルシステム呼び出し
	nResult = FS_Flush();

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $57 - メディア交換チェック
// 
//	in	(offset	size)
//		 0	1.b	定数(26)
//		 1	1.b	ユニット番号
//		 2	1.b	コマンド($57/$d7)
//	out	(offset	size)
//		 3	1.b	エラーコード(下位)
//		 4	1.b	〃			(上位)
//		18	1.l	リザルトステータス
//
//    メディアが交換されたか否かを調べる. 交換されていた場合のフォーマット
//  確認はこのコマンド内で行うこと.
//
//---------------------------------------------------------------------------
DWORD CheckMedia(void)
{
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// ファイルシステム呼び出し
	nResult = FS_CheckMedia();

	return nResult;
}

//---------------------------------------------------------------------------
//
//  $58 - 排他制御
// 
//	in	(offset	size)
//		 1	1.B	ユニット番号
//
//---------------------------------------------------------------------------
DWORD Lock(void)
{
	int nResult;

	ASSERT(this);
	ASSERT(fs);

	// ファイルシステム呼び出し
	nResult = FS_Lock();

	return nResult;
}

//---------------------------------------------------------------------------
//
//  コマンド実行
//
//---------------------------------------------------------------------------
DWORD ExecuteCommand()
{
	ASSERT(this);

	// エラー情報クリア
	SetReqByte(3, 0);
	SetReqByte(4, 0);

	// コマンド番号
	command = (DWORD)GetReqByte(2);	// ビット7はベリファイフラグ

	// ユニット番号
	unit = GetReqByte(1);

	// コマンド分岐
	switch (command & 0x7F) {
		case 0x40: return InitDevice();	// $40 - デバイス起動
		case 0x41: return CheckDir();	// $41 - ディレクトリチェック
		case 0x42: return MakeDir();	// $42 - ディレクトリ作成
		case 0x43: return RemoveDir();	// $43 - ディレクトリ削除
		case 0x44: return Rename();		// $44 - ファイル名変更
		case 0x45: return Delete();		// $45 - ファイル削除
		case 0x46: return Attribute();	// $46 - ファイル属性取得/設定
		case 0x47: return Files();		// $47 - ファイル検索
		case 0x48: return NFiles();		// $48 - ファイル次検索
		case 0x49: return Create();		// $49 - ファイル作成
		case 0x4A: return Open();		// $4A - ファイルオープン
		case 0x4B: return Close();		// $4B - ファイルクローズ
		case 0x4C: return Read();		// $4C - ファイル読み込み
		case 0x4D: return Write();		// $4D - ファイル書き込み
		case 0x4E: return Seek();		// $4E - ファイルシーク
		case 0x4F: return TimeStamp();	// $4F - ファイル更新時刻の取得/設定
		case 0x50: return GetCapacity();// $50 - 容量取得
		case 0x51: return CtrlDrive();	// $51 - ドライブ制御/状態検査
		case 0x52: return GetDPB();		// $52 - DPB取得
		case 0x53: return DiskRead();	// $53 - セクタ読み込み
		case 0x54: return DiskWrite();	// $54 - セクタ書き込み
		case 0x55: return Ioctrl();		// $55 - IOCTRL
		case 0x56: return Flush();		// $56 - フラッシュ
		case 0x57: return CheckMedia();	// $57 - メディア交換チェック
		case 0x58: return Lock();		// $58 - 排他制御
	}

	return FS_FATAL_INVALIDCOMMAND;
}

//---------------------------------------------------------------------------
//
//  初期化
//
//---------------------------------------------------------------------------
BOOL Init()
{
	ASSERT(this);

	return SCSI_Init();
}

//---------------------------------------------------------------------------
//
//  実行
//
//---------------------------------------------------------------------------
void Process(DWORD nA5)
{
	ASSERT(this);
	ASSERT(nA5 <= 0xFFFFFF);
	ASSERT(m_bAlloc);
	ASSERT(m_bFree);

	// リクエストヘッダのアドレス
	request = (BYTE*)nA5;

	// コマンド実行
	SetResult(ExecuteCommand());
}
