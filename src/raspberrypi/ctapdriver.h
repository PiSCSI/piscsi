//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
//
//	[ TAP Driver ]
//
//---------------------------------------------------------------------------

#if !defined(ctapdriver_h)
#define ctapdriver_h

#ifndef ETH_FRAME_LEN
#define ETH_FRAME_LEN 1514
#endif

//===========================================================================
//
//	Tapドライバ
//
//===========================================================================
class CTapDriver
{
public:
	// 基本ファンクション
	CTapDriver();
										// コンストラクタ
	BOOL FASTCALL Init();
										// 初期化
	void FASTCALL Cleanup();
										// クリーンアップ
	void FASTCALL GetMacAddr(BYTE *mac);
										// MACアドレス取得
	int FASTCALL Rx(BYTE *buf);
										// 受信
	int FASTCALL Tx(BYTE *buf, int len);
										// 送信

private:
	BYTE m_MacAddr[6];
										// MACアドレス
	BOOL m_bTxValid;
										// 送信有効フラグ
	int m_hTAP;
										// ディスクプリタ
};

#endif	// ctapdriver_h
