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
//	Sense Keys and Additional Sense Codes
//  (See https://www.t10.org/lists/1spc-lst.htm)
//
//===========================================================================
class ERROR_CODES
{
public:
	enum sense_key : int {
		NO_SENSE = 0x00,
		RECOVERED_ERROR = 0x01,
		NOT_READY = 0x02,
		MEDIUM_ERROR = 0x03,
		HARDWARE_ERROR = 0x04,
		ILLEGAL_REQUEST = 0x05,
		UNIT_ATTENTION = 0x06,
		DATA_PROTECT = 0x07,
		BLANK_CHECK = 0x08,
		VENDOR_SPECIFIC = 0x09,
		COPY_ABORTED = 0x0a,
		ABORTED_COMMAND = 0x0b,
		VOLUME_OVERFLOW = 0x0d,
		MISCOMPARE = 0x0e,
		COMPLETED = 0x0f
	};

	enum asc : int {
		NO_ADDITIONAL_SENSE_INFORMATION = 0x00,
		INVALID_COMMAND_OPERATION_CODE = 0x20,
		LBA_OUT_OF_RANGE = 0x21,
		INVALID_FIELD_IN_CDB = 0x24,
		INVALID_LUN = 0x25,
		WRITE_PROTECTED = 0x27,
		MEDIUM_NOT_PRESENT = 0x3a
	};
};

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

	BUS() { };
	virtual ~BUS() { };

	// Basic Functions
	// 基本ファンクション
	virtual BOOL Init(mode_e mode) = 0;
										// 初期化
	virtual void Reset() = 0;
										// リセット
	virtual void Cleanup() = 0;
										// クリーンアップ
	phase_t GetPhase();
										// フェーズ取得

	static phase_t GetPhase(DWORD mci)
	{
		return phase_table[mci];
	}
										// フェーズ取得

	static const char* GetPhaseStrRaw(phase_t current_phase);
										// Get the string phase name, based upon the raw data

	// Extract as specific pin field from a raw data capture
	static inline DWORD GetPinRaw(DWORD raw_data, DWORD pin_num)
	{
		return ((raw_data >> pin_num) & 1);
	}

	virtual bool GetBSY() = 0;
										// BSYシグナル取得
	virtual void SetBSY(bool ast) = 0;
										// BSYシグナル設定

	virtual BOOL GetSEL() = 0;
										// SELシグナル取得
	virtual void SetSEL(BOOL ast) = 0;
										// SELシグナル設定

	virtual BOOL GetATN() = 0;
										// ATNシグナル取得
	virtual void SetATN(BOOL ast) = 0;
										// ATNシグナル設定

	virtual BOOL GetACK() = 0;
										// ACKシグナル取得
	virtual void SetACK(BOOL ast) = 0;
										// ACKシグナル設定

	virtual BOOL GetRST() = 0;
										// RSTシグナル取得
	virtual void SetRST(BOOL ast) = 0;
										// RSTシグナル設定

	virtual BOOL GetMSG() = 0;
										// MSGシグナル取得
	virtual void SetMSG(BOOL ast) = 0;
										// MSGシグナル設定

	virtual BOOL GetCD() = 0;
										// CDシグナル取得
	virtual void SetCD(BOOL ast) = 0;
										// CDシグナル設定

	virtual BOOL GetIO() = 0;
										// IOシグナル取得
	virtual void SetIO(BOOL ast) = 0;
										// IOシグナル設定

	virtual BOOL GetREQ() = 0;
										// REQシグナル取得
	virtual void SetREQ(BOOL ast) = 0;
										// REQシグナル設定

	virtual BYTE GetDAT() = 0;
										// データシグナル取得
	virtual void SetDAT(BYTE dat) = 0;
										// データシグナル設定
	virtual BOOL GetDP() = 0;
										// パリティシグナル取得

	virtual int CommandHandShake(BYTE *buf) = 0;
										// コマンド受信ハンドシェイク
	virtual int ReceiveHandShake(BYTE *buf, int count) = 0;
										// データ受信ハンドシェイク
	virtual int SendHandShake(BYTE *buf, int count, int delay_after_bytes) = 0;
										// データ送信ハンドシェイク


	virtual BOOL GetSignal(int pin) = 0;
										// Get SCSI input signal value
	virtual void SetSignal(int pin, BOOL ast) = 0;
										// Set SCSI output signal value
	static const int SEND_NO_DELAY = -1;
										// Passed into SendHandShake when we don't want to delay
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
