//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	[ SCSI Common Functionality ]
//
//---------------------------------------------------------------------------

#if !defined(scsi_h)
#define scsi_h

//===========================================================================
//
//	SASI/SCSI Bus
//
//===========================================================================
class BUS
{
public:
	// Operation modes definition
	enum mode_e {
		TARGET = 0,
		INITIATOR = 1,
		MONITOR = 2,
	};

	//	Phase definition
	enum phase_t : BYTE {
		busfree,						// バスフリーフェーズ
		arbitration,					// アービトレーションフェーズ
		selection,						// セレクションフェーズ
		reselection,					// リセレクションフェーズ
		command,						// コマンドフェーズ
		execute,						// 実行フェーズ  Execute is an extension of the command phase
		datain,							// データイン
		dataout,						// データアウト
		status,							// ステータスフェーズ
		msgin,							// メッセージフェーズ
		msgout,							// メッセージアウトフェーズ
		reserved						// 未使用/リザーブ
	};

    // Basic Functions
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

	static const char* FASTCALL GetPhaseStrRaw(phase_t current_phase);
										// Get the string phase name, based upon the raw data

	// Extract as specific pin field from a raw data capture
	static inline DWORD GetPinRaw(DWORD raw_data, DWORD pin_num)
	{
		return ((raw_data >> pin_num) & 1);
	}

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


	virtual BOOL FASTCALL GetSignal(int pin) = 0;
										// Get SCSI input signal value
	virtual void FASTCALL SetSignal(int pin, BOOL ast) = 0;
										// Set SCSI output signal value
protected:
	phase_t m_current_phase = phase_t::reserved;

private:
	static const phase_t phase_table[8];
										// フェーズテーブル

	static const char* phase_str_table[];
};


typedef struct __attribute__((packed)) {
	BYTE operation_code;
	BYTE misc_cdb_information;
    BYTE page_code;
	WORD length;
	BYTE control;
} scsi_cdb_6_byte_t;


typedef struct __attribute__((packed)) {
	BYTE operation_code;
	BYTE service_action;
    DWORD logical_block_address;
	BYTE misc_cdb_information;
	WORD length;
	BYTE control;
} scsi_cdb_10_byte_t;

typedef struct __attribute__((packed)) {
	BYTE operation_code;
	BYTE service_action;
    DWORD logical_block_address;
	DWORD length;
	BYTE misc_cdb_information;
	BYTE control;
} scsi_cdb_12_byte_t;

typedef struct __attribute__((packed)) {
	BYTE operation_code;
	BYTE service_action;
    DWORD logical_block_address;
	DWORD length;
	BYTE misc_cdb_information;
	BYTE control;
} scsi_cdb_16_byte_t;

typedef struct __attribute__((packed)) {
	BYTE operation_code;
	BYTE reserved;
	BYTE page_code;
	WORD allocation_length;
	BYTE control;
} scsi_cmd_inquiry_t;

typedef struct __attribute__((packed)) {
	BYTE operation_code;
	BYTE lba_msb_bits_4_0;
	WORD logical_block_address;
	BYTE transfer_length;
	BYTE control;
} scsi_cmd_read_6_t;

typedef struct __attribute__((packed)) {
	BYTE operation_code;
	BYTE flags;
	DWORD logical_block_address;
	BYTE group_number;
	WORD transfer_length;
	BYTE control;
} scsi_cmd_read_10_t;

typedef struct __attribute__((packed)) {
	BYTE operation_code;
	BYTE flags;
	DWORD logical_block_address;
	DWORD transfer_length;
	BYTE group_number;
	BYTE control;
} scsi_cmd_read_12_t;

typedef struct __attribute__((packed)) {
	BYTE operation_code;
	BYTE descriptor_format;
	WORD reserved;
	BYTE allocation_length;
	BYTE control;
} scsi_cmd_request_sense_t;


#endif	// scsi_h
