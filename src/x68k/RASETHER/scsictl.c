//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technorogy.
//  Copyright (C) 2016-2017 GIMONS
//	[ RaSCSI イーサーネット SCSI制御部 ]
//
//  Based on
//    Neptune-X board driver for  Human-68k(ESP-X)   version 0.03
//    Programed 1996-7 by Shi-MAD.
//    Special thanks to  Niggle, FIRST, yamapu ...
//
//---------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <sys/dos.h>
#include <sys/scsi.h>
#include <iocslib.h>
#include "main.h"
#include "scsictl.h"

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

typedef struct
{
	INQUIRY_T info;
	char options[8];
} INQUIRYOPT_T;

#define MFP_AEB		0xe88003
#define MFP_IERB	0xe88009
#define MFP_IMRB	0xe88015

// asmsub.s 内のサブルーチン
extern void DI();
extern void EI();

volatile short* iocsexec = (short*)0xa0e;	// IOCS実行中ワーク
struct trans_counter trans_counter;			// 送受信カウンタ
unsigned char rx_buff[2048];				// 受信バッファ
int imr;									// 割り込み許可フラグ
int scsistop;								// SCSI停止中フラグ
int intr_type;								// 割り込み種別(0:V-DISP 1:TimerA)
int poll_interval;							// ポーリング間隔(設定)
int poll_current;							// ポーリング間隔(現在)
int idle;									// アイドルカウンタ

#define POLLING_SLEEP	255	// 4-5s

/************************************************
 *   MACアドレス取得命令発行                    *
 ************************************************/
int SCSI_GETMACADDR(unsigned char *mac)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	cmd[8] = 6;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	if (S_DATAIN(6, mac) != 0) {
#else
	if (S_DATAIN_P(6, mac) != 0) {
#endif
		return 0;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	return 1;
}

/************************************************
 *   MACアドレス設定命令発行                    *
 ************************************************/
int SCSI_SETMACADDR(const unsigned char *mac)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x2a;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	cmd[8] = 6;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	S_DATAOUT(6, mac);
#else
	S_DATAOUT_P(6, mac);
#endif

	S_STSIN(&sts);
	S_MSGIN(&msg);
	
	return 1;
}

/************************************************
 *   パケット受信サイズ取得命令発行             *
 ************************************************/
int SCSI_GETPACKETLEN(int *len)
{
	unsigned char cmd[10];
	unsigned char buf[2];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	cmd[8] = 2;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	if (S_DATAIN(2, buf) != 0) {
#else
	if (S_DATAIN_P(2, buf) != 0) {
#endif
		return 0;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	*len = (int)(buf[0] << 8) + (int)(buf[1]);

	return 1;
}

/************************************************
 *   パケット受信命令発行                       *
 ************************************************/
int SCSI_GETPACKETBUF(unsigned char *buf, int len)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = (unsigned char)(len >> 16);
	cmd[7] = (unsigned char)(len >> 8);
	cmd[8] = (unsigned char)len;
	cmd[9] = 1;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	if (S_DATAIN(len, buf) != 0) {
#else
	if (S_DATAIN_P(len, buf) != 0) {
#endif
		return 0;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	return 1;
}

/************************************************
 *   パケット送信命令発行                       *
 ************************************************/
int SCSI_SENDPACKET(const unsigned char *buf, int len)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x2a;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = (unsigned char)(len >> 16);
	cmd[7] = (unsigned char)(len >> 8);
	cmd[8] = (unsigned char)len;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	S_DATAOUT(len, buf);
#else
	S_DATAOUT_P(len, buf);
#endif
	S_STSIN(&sts);
	S_MSGIN(&msg);
	
	return 1;
}

/************************************************
 *   MACアドレス取得                            *
 ************************************************/
int GetMacAddr(struct eaddr* buf)
{
	if (SCSI_GETMACADDR(buf->eaddr) != 0) {
		return 0;
	} else {
		return -1;
	}
}

/************************************************
 *   MACアドレス設定                            *
 ************************************************/
int SetMacAddr(const struct eaddr* data)
{
	if (SCSI_SETMACADDR(data->eaddr) != 0) {
		return 0;
	} else {
		return -1;
	}
}

/************************************************
 *  RaSCSI検索                                  *
 ************************************************/
int SearchRaSCSI()
{
	int i;
	INQUIRYOPT_T inq;

	for (i = 0; i <= 7; i++) {
		// BRIDGEデバイス検索
		if (S_INQUIRY(sizeof(INQUIRY_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}

		if (memcmp(&(inq.info.ProductID), "RASCSI BRIDGE", 13) != 0) {
			continue;
		}

		// TAP初期化状態を取得
		if (S_INQUIRY(sizeof(INQUIRYOPT_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}
		
		if (inq.options[1] != '1') {
			continue;
		}

		// SCSI ID確定
		scsiid = i;
		return 0;
	}

	return -1;
}

/************************************************
 *  RaSCSI初期化 関数                           *
 ************************************************/
int InitRaSCSI(void)
{
#ifdef MULTICAST
	unsigned char multicast_table[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
#endif

	imr = 0;
	scsistop = 0;
	poll_current = -1;
	idle = 0;
	
	return 0;
}

/************************************************
 *  RaSCSI 割り込み処理 関数(ポーリング)        *
 ************************************************/
void interrupt IntProcess(void)
{
	unsigned char phase;
	unsigned int len;
	int type;
	int_handler func;
	int i;

	// V-DISP GPIP割り込みはアイドルカウンタで制御
	if (intr_type == 0) {
		// アイドル加算
		idle++;
		
		// 次回の処理予定に到達していないならスキップ
		if (idle < poll_current) {
			return;
		}

		// アイドルカウンタをクリア
		idle = 0;
	}
	
	// 割り込み開始

	// 割り込み許可の時だけ
	if (imr == 0) {
		return;
	}

	// IOCS実行中ならばスキップ
	if (*iocsexec != -1) {
		return;
	}

	// 受信処理中は割り込み禁止
	DI ();

	// バスフリーの時だけ
	phase = S_PHASE();
	if (phase != 0) {
		// 終了
		goto ei_exit;
	}	

	// 受信処理
	if (SCSI_GETPACKETLEN(&len) == 0) {
		// RaSCSI停止中
		scsistop = 1;

		// ポーリング間隔の再設定(寝る)
		UpdateIntProcess(POLLING_SLEEP);

		// 終了
		goto ei_exit;
	}
	
	// RaSCSIは動作中
	if (scsistop) {
		scsistop = 0;

		// ポーリング間隔の再設定(急ぐ)
		UpdateIntProcess(poll_interval);
	}
	
	// パケットは到着してなかった
	if (len == 0) {
		// 終了
		goto ei_exit;
	}

	// 受信バッファメモリへパケット転送
	if (SCSI_GETPACKETBUF(rx_buff, len) == 0) {
		// 失敗
		goto ei_exit;
	}

	// 割り込み許可
	EI ();
	
	// パケットタイプでデータ分別
	type = rx_buff[12] * 256 + rx_buff[13];
	i = 0;
	while ((func = SearchList2(type, i))) {
		i++;
		func(len, (void*)&rx_buff, ID_EN0);
	}
	trans_counter.recv_byte += len;
	return;

ei_exit:
	// 割り込み許可
	EI ();
}

/************************************************
 *  RaSCSI パケット送信 関数 (ne.sから)         *
 ************************************************/
int SendPacket(int len, const unsigned char* data)
{
	if (len < 1) {
		return 0;
	}
	
	if (len > 1514)	{		// 6 + 6 + 2 + 1500
		return -1;			// エラー
	}
	
	// RaSCSI停止中のようならエラー
	if (scsistop) {
		return -1;
	}

	// 送信処理中は割り込み禁止
	DI ();

	// 送信処理と送信フラグアップ
	if (SCSI_SENDPACKET(data, len) == 0) {
		// 割り込み許可
		EI ();
		return -1;
	}

	// 割り込み許可
	EI ();

	// 送信依頼済み
	trans_counter.send_byte += len;
	
	return 0;
}

/************************************************
 *  RaSCSI 割り込み許可、不許可設定 関数        *
 ************************************************/
int SetPacketReception(int i)
{
	imr = i;
	return 0;
}

/************************************************
 * RaSCSI 割り込み処理登録                      *
 ************************************************/
void RegisterIntProcess(int n)
{
	volatile unsigned char *p;

	// ポーリング間隔(現在)の更新とアイドルカウンタクリア
	poll_current = n;
	idle = 0;
	
	if (intr_type == 0) {
		// V-DISP GPIP割り込みベクタを書き換えて
		// 割り込みを有効にする
		B_INTVCS(0x46, (int)IntProcess);
		p = (unsigned char *)MFP_AEB;
		*p = *p | 0x10;
		p = (unsigned char *)MFP_IERB;
		*p = *p | 0x40;
		p = (unsigned char *)MFP_IMRB;
		*p = *p | 0x40;
	} else if (intr_type == 1) {
		// TimerAはカウントモードを設定
		VDISPST(NULL, 0, 0);
		VDISPST(IntProcess, 0, poll_current);
	}
}

/************************************************
 * RaSCSI 割り込み処理変更                      *
 ************************************************/
void UpdateIntProcess(int n)
{
	// ポーリング間隔(現在)と同じなら更新しない
	if (n == poll_current) {
		return;
	}
	
	// ポーリング間隔(現在)の更新とアイドルカウンタクリア
	poll_current = n;
	idle = 0;
	
	if (intr_type == 1) {
		// TimerAは再登録必要
		VDISPST(NULL, 0, 0);
		VDISPST(IntProcess, 0, poll_current);
	}
}

// EOF
