//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	[ SCSI共通 ]
//
//---------------------------------------------------------------------------

#if !defined(scsi_h)
#define scsi_h

//===========================================================================
//
//	SASI/SCSI バス
//
//===========================================================================
class BUS
{
public:
	// 動作モード定義
	enum mode_e {
		TARGET = 0,
		INITIATOR = 1,
		MONITOR = 2,
	};

	//	フェーズ定義
	enum phase_t {
		busfree,						// バスフリーフェーズ
		arbitration,					// アービトレーションフェーズ
		selection,						// セレクションフェーズ
		reselection,					// リセレクションフェーズ
		command,						// コマンドフェーズ
		execute,						// 実行フェーズ
		datain,							// データイン
		dataout,						// データアウト
		status,							// ステータスフェーズ
		msgin,							// メッセージフェーズ
		msgout,							// メッセージアウトフェーズ
		reserved						// 未使用/リザーブ
	};

	// 基本ファンクション
	virtual BOOL FASTCALL Init(mode_e mode) = 0;
										// 初期化
	virtual void FASTCALL Reset() = 0;
										// リセット
	virtual void FASTCALL Cleanup() = 0;
										// クリーンアップ
	phase_t FASTCALL GetPhase();
										// フェーズ取得

	static phase_t FASTCALL GetPhase(DWORD mci)
	{
		return phase_table[mci];
	}
										// フェーズ取得

	virtual DWORD FASTCALL Aquire() = 0;
										// 信号取り込み

	virtual BOOL FASTCALL GetBSY() = 0;
										// BSYシグナル取得
	virtual void FASTCALL SetBSY(BOOL ast) = 0;
										// BSYシグナル設定

	virtual BOOL FASTCALL GetSEL() = 0;
										// SELシグナル取得
	virtual void FASTCALL SetSEL(BOOL ast) = 0;
										// SELシグナル設定

	virtual BOOL FASTCALL GetATN() = 0;
										// ATNシグナル取得
	virtual void FASTCALL SetATN(BOOL ast) = 0;
										// ATNシグナル設定

	virtual BOOL FASTCALL GetACK() = 0;
										// ACKシグナル取得
	virtual void FASTCALL SetACK(BOOL ast) = 0;
										// ACKシグナル設定

	virtual BOOL FASTCALL GetRST() = 0;
										// RSTシグナル取得
	virtual void FASTCALL SetRST(BOOL ast) = 0;
										// RSTシグナル設定

	virtual BOOL FASTCALL GetMSG() = 0;
										// MSGシグナル取得
	virtual void FASTCALL SetMSG(BOOL ast) = 0;
										// MSGシグナル設定

	virtual BOOL FASTCALL GetCD() = 0;
										// CDシグナル取得
	virtual void FASTCALL SetCD(BOOL ast) = 0;
										// CDシグナル設定

	virtual BOOL FASTCALL GetIO() = 0;
										// IOシグナル取得
	virtual void FASTCALL SetIO(BOOL ast) = 0;
										// IOシグナル設定

	virtual BOOL FASTCALL GetREQ() = 0;
										// REQシグナル取得
	virtual void FASTCALL SetREQ(BOOL ast) = 0;
										// REQシグナル設定

	virtual BYTE FASTCALL GetDAT() = 0;
										// データシグナル取得
	virtual void FASTCALL SetDAT(BYTE dat) = 0;
										// データシグナル設定
	virtual BOOL FASTCALL GetDP() = 0;
										// パリティシグナル取得

	virtual int FASTCALL CommandHandShake(BYTE *buf) = 0;
										// コマンド受信ハンドシェイク
	virtual int FASTCALL ReceiveHandShake(BYTE *buf, int count) = 0;
										// データ受信ハンドシェイク
	virtual int FASTCALL SendHandShake(BYTE *buf, int count) = 0;
										// データ送信ハンドシェイク

private:
	static const phase_t phase_table[8];
										// フェーズテーブル
};

#endif	// scsi_h
