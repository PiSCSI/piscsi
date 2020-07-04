//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ GPIO-SCSIバス ]
//
//---------------------------------------------------------------------------

#if !defined(gpiobus_h)
#define gpiobus_h

#include "scsi.h"

//---------------------------------------------------------------------------
//
//	接続方法定義の選択
//
//---------------------------------------------------------------------------
//#define CONNECT_TYPE_STANDARD		// 標準(SCSI論理,標準ピンアサイン)
//#define CONNECT_TYPE_FULLSPEC		// フルスペック(SCSI論理,標準ピンアサイン)
//#define CONNECT_TYPE_AIBOM		// AIBOM版(正論理,固有ピンアサイン)
//#define CONNECT_TYPE_GAMERNIUM	// GAMERnium.com版(標準論理,固有ピンアサイン)

//---------------------------------------------------------------------------
//
//	信号制御論理及びピンアサインカスタマイズ
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	SIGNAL_CONTROL_MODE:信号制御モード選択
//	 Version1.22から信号制御の論理をカスタマイズできます。
//
//	 0:SCSI論理仕様
//	  直結またはHPに公開した74LS641-1等を使用する変換基板
//	  アーサート:0V
//	  ネゲート  :オープンコレクタ出力(バスから切り離す)
//
//	 1:負論理仕様(負論理->SCSI論理への変換基板を使用する場合)
//	  現時点でこの仕様による変換基板は存在しません
//	  アーサート:0V   -> (CONVERT) -> 0V
//	  ネゲート  :3.3V -> (CONVERT) -> オープンコレクタ出力
//
//	 2:正論理仕様(正論理->SCSI論理への変換基板を使用する場合)
//	  RaSCSI Adapter Rev.C @132sync等
//
//	  アーサート:3.3V -> (CONVERT) -> 0V
//	  ネゲート  :0V   -> (CONVERT) -> オープンコレクタ出力
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	制御信号ピンアサイン設定
//	 制御信号に対するGPIOピンのマッピングテーブルです。
//
//	 制御信号
//	  PIN_ACT
//	    SCSIコマンドを処理中の状態を示す信号のピン番号。
//	  PIN_ENB
//	    起動から終了の間の有効信号を示す信号のピン番号。
//	  PIN_TAD
//	    ターゲット信号(BSY,IO,CD,MSG,REG)の入出力方向を示す信号のピン番号。
//	  PIN_IND
//	    イニシーエータ信号(SEL,ATN,RST,ACK)の入出力方向を示す信号のピン番号。
//	  PIN_DTD
//	    データ信号(DT0...DT7,DP)の入出力方向を示す信号のピン番号。
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	制御信号出力論理
//	  0V:FALSE  3.3V:TRUEで指定します。
//
//	  ACT_ON
//	    PIN_ACT信号の論理です。
//	  ENB_ON
//	    PIN_ENB信号の論理です。
//	  TAD_IN
//	    PIN_TAD入力方向時の論理です。
//	  IND_IN
//	    PIN_ENB入力方向時の論理です。
//    DTD_IN
//	    PIN_ENB入力方向時の論理です。
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	SCSI信号ピンアサイン設定
//	  SCSIの信号に対するGPIOピンのマッピングテーブルです。
//	  PIN_DT0～PIN_SEL
//
//---------------------------------------------------------------------------

#ifdef CONNECT_TYPE_STANDARD
//
// RaSCSI 標準(SCSI論理,標準ピンアサイン)
//
#define CONNECT_DESC "STANDARD"				// 起動時メッセージ

// 信号制御モード選択
#define SIGNAL_CONTROL_MODE 0				// SCSI論理仕様

// 制御信号ピンアサイン(-1の場合は制御無し)
#define	PIN_ACT		4						// ACTIVE
#define	PIN_ENB		5						// ENABLE
#define PIN_IND		-1						// INITIATOR CTRL DIRECTION
#define PIN_TAD		-1						// TARGET CTRL DIRECTION
#define PIN_DTD		-1						// DATA DIRECTION

// 制御信号出力論理
#define ACT_ON		TRUE					// ACTIVE SIGNAL ON
#define ENB_ON		TRUE					// ENABLE SIGNAL ON
#define IND_IN		FALSE					// INITIATOR SIGNAL INPUT
#define TAD_IN		FALSE					// TARGET SIGNAL INPUT
#define DTD_IN		TRUE					// DATA SIGNAL INPUT

// SCSI信号ピンアサイン
#define	PIN_DT0		10						// データ0
#define	PIN_DT1		11						// データ1
#define	PIN_DT2		12						// データ2
#define	PIN_DT3		13						// データ3
#define	PIN_DT4		14						// データ4
#define	PIN_DT5		15						// データ5
#define	PIN_DT6		16						// データ6
#define	PIN_DT7		17						// データ7
#define	PIN_DP		18						// パリティ
#define	PIN_ATN		19						// ATN
#define	PIN_RST		20						// RST
#define	PIN_ACK		21						// ACK
#define	PIN_REQ		22						// REQ
#define	PIN_MSG		23						// MSG
#define	PIN_CD		24						// CD
#define	PIN_IO		25						// IO
#define	PIN_BSY		26						// BSY
#define	PIN_SEL		27						// SEL
#endif

#ifdef CONNECT_TYPE_FULLSPEC
//
// RaSCSI 標準(SCSI論理,標準ピンアサイン)
//
#define CONNECT_DESC "FULLSPEC"				// 起動時メッセージ

// 信号制御モード選択
#define SIGNAL_CONTROL_MODE 0				// SCSI論理仕様

// 制御信号ピンアサイン(-1の場合は制御無し)
#define	PIN_ACT		4						// ACTIVE
#define	PIN_ENB		5						// ENABLE
#define PIN_IND		6						// INITIATOR CTRL DIRECTION
#define PIN_TAD		7						// TARGET CTRL DIRECTION
#define PIN_DTD		8						// DATA DIRECTION

// 制御信号出力論理
#define ACT_ON		TRUE					// ACTIVE SIGNAL ON
#define ENB_ON		TRUE					// ENABLE SIGNAL ON
#define IND_IN		FALSE					// INITIATOR SIGNAL INPUT
#define TAD_IN		FALSE					// TARGET SIGNAL INPUT
#define DTD_IN		TRUE					// DATA SIGNAL INPUT

// SCSI信号ピンアサイン
#define	PIN_DT0		10						// データ0
#define	PIN_DT1		11						// データ1
#define	PIN_DT2		12						// データ2
#define	PIN_DT3		13						// データ3
#define	PIN_DT4		14						// データ4
#define	PIN_DT5		15						// データ5
#define	PIN_DT6		16						// データ6
#define	PIN_DT7		17						// データ7
#define	PIN_DP		18						// パリティ
#define	PIN_ATN		19						// ATN
#define	PIN_RST		20						// RST
#define	PIN_ACK		21						// ACK
#define	PIN_REQ		22						// REQ
#define	PIN_MSG		23						// MSG
#define	PIN_CD		24						// CD
#define	PIN_IO		25						// IO
#define	PIN_BSY		26						// BSY
#define	PIN_SEL		27						// SEL
#endif

#ifdef CONNECT_TYPE_AIBOM
//
// RaSCSI Adapter あいぼむ版
//

#define CONNECT_DESC "AIBOM PRODUCTS version"		// 起動時メッセージ

// 信号制御モード選択
#define SIGNAL_CONTROL_MODE 2				// SCSI正論理仕様

// 制御信号出力論理
#define ACT_ON		TRUE					// ACTIVE SIGNAL ON
#define ENB_ON		TRUE					// ENABLE SIGNAL ON
#define IND_IN		FALSE					// INITIATOR SIGNAL INPUT
#define TAD_IN		FALSE					// TARGET SIGNAL INPUT
#define DTD_IN		FALSE					// DATA SIGNAL INPUT

// 制御信号ピンアサイン(-1の場合は制御無し)
#define	PIN_ACT		4						// ACTIVE
#define	PIN_ENB		17						// ENABLE
#define PIN_IND		27						// INITIATOR CTRL DIRECTION
#define PIN_TAD		-1						// TARGET CTRL DIRECTION
#define PIN_DTD		18						// DATA DIRECTION

// SCSI信号ピンアサイン
#define	PIN_DT0		6						// データ0
#define	PIN_DT1		12						// データ1
#define	PIN_DT2		13						// データ2
#define	PIN_DT3		16						// データ3
#define	PIN_DT4		19						// データ4
#define	PIN_DT5		20						// データ5
#define	PIN_DT6		26						// データ6
#define	PIN_DT7		21						// データ7
#define	PIN_DP		5						// パリティ
#define	PIN_ATN		22						// ATN
#define	PIN_RST		25						// RST
#define	PIN_ACK		10						// ACK
#define	PIN_REQ		7						// REQ
#define	PIN_MSG		9						// MSG
#define	PIN_CD		11						// CD
#define	PIN_IO		23						// IO
#define	PIN_BSY		24						// BSY
#define	PIN_SEL		8						// SEL
#endif

#ifdef CONNECT_TYPE_GAMERNIUM
//
// RaSCSI Adapter GAMERnium.com版
//

#define CONNECT_DESC "GAMERnium.com version"// 起動時メッセージ

// 信号制御モード選択
#define SIGNAL_CONTROL_MODE 0				// SCSI論理仕様

// 制御信号出力論理
#define ACT_ON		TRUE					// ACTIVE SIGNAL ON
#define ENB_ON		TRUE					// ENABLE SIGNAL ON
#define IND_IN		FALSE					// INITIATOR SIGNAL INPUT
#define TAD_IN		FALSE					// TARGET SIGNAL INPUT
#define DTD_IN		TRUE					// DATA SIGNAL INPUT

// 制御信号ピンアサイン(-1の場合は制御無し)
#define	PIN_ACT		14						// ACTIVE
#define	PIN_ENB		6						// ENABLE
#define PIN_IND		7						// INITIATOR CTRL DIRECTION
#define PIN_TAD		8						// TARGET CTRL DIRECTION
#define PIN_DTD		5						// DATA DIRECTION

// SCSI信号ピンアサイン
#define	PIN_DT0		21						// データ0
#define	PIN_DT1		26						// データ1
#define	PIN_DT2		20						// データ2
#define	PIN_DT3		19						// データ3
#define	PIN_DT4		16						// データ4
#define	PIN_DT5		13						// データ5
#define	PIN_DT6		12						// データ6
#define	PIN_DT7		11						// データ7
#define	PIN_DP		25						// パリティ
#define	PIN_ATN		10						// ATN
#define	PIN_RST		22						// RST
#define	PIN_ACK		24						// ACK
#define	PIN_REQ		15						// REQ
#define	PIN_MSG		17						// MSG
#define	PIN_CD		18						// CD
#define	PIN_IO		4						// IO
#define	PIN_BSY		27						// BSY
#define	PIN_SEL		23						// SEL
#endif

//---------------------------------------------------------------------------
//
//	定数宣言(GPIO)
//
//---------------------------------------------------------------------------
#define SYST_OFFSET		0x00003000
#define IRPT_OFFSET		0x0000B200
#define ARMT_OFFSET		0x0000B400
#define PADS_OFFSET		0x00100000
#define GPIO_OFFSET		0x00200000
#define QA7_OFFSET		0x01000000
#define GPIO_INPUT		0
#define GPIO_OUTPUT		1
#define GPIO_PULLNONE	0
#define GPIO_PULLDOWN	1
#define GPIO_PULLUP		2
#define GPIO_FSEL_0		0
#define GPIO_FSEL_1		1
#define GPIO_FSEL_2		2
#define GPIO_FSEL_3		3
#define GPIO_SET_0		7
#define GPIO_CLR_0		10
#define GPIO_LEV_0		13
#define GPIO_EDS_0		16
#define GPIO_REN_0		19
#define GPIO_FEN_0		22
#define GPIO_HEN_0		25
#define GPIO_LEN_0		28
#define GPIO_AREN_0		31
#define GPIO_AFEN_0		34
#define GPIO_PUD		37
#define GPIO_CLK_0		38
#define GPIO_GPPINMUXSD	52
#define GPIO_PUPPDN0	57
#define GPIO_PUPPDN1	58
#define GPIO_PUPPDN3	59
#define GPIO_PUPPDN4	60
#define PAD_0_27		11
#define SYST_CS			0
#define SYST_CLO		1
#define SYST_CHI		2
#define SYST_C0			3
#define SYST_C1			4
#define SYST_C2			5
#define SYST_C3			6
#define ARMT_LOAD		0
#define ARMT_VALUE		1
#define ARMT_CTRL		2
#define ARMT_CLRIRQ		3
#define ARMT_RAWIRQ		4
#define ARMT_MSKIRQ		5
#define ARMT_RELOAD		6
#define ARMT_PREDIV		7
#define ARMT_FREERUN	8
#define IRPT_PND_IRQ_B	0
#define IRPT_PND_IRQ_1	1
#define IRPT_PND_IRQ_2	2
#define IRPT_FIQ_CNTL	3
#define IRPT_ENB_IRQ_1	4
#define IRPT_ENB_IRQ_2	5
#define IRPT_ENB_IRQ_B	6
#define IRPT_DIS_IRQ_1	7
#define IRPT_DIS_IRQ_2	8
#define IRPT_DIS_IRQ_B	9
#define QA7_CORE0_TINTC	16
#define GPIO_IRQ		(32 + 20)	// GPIO3

#define GPIO_INEDGE ((1 << PIN_BSY) | \
					 (1 << PIN_SEL) | \
					 (1 << PIN_ATN) | \
					 (1 << PIN_ACK) | \
					 (1 << PIN_RST))

#define GPIO_MCI	((1 << PIN_MSG) | \
					 (1 << PIN_CD) | \
					 (1 << PIN_IO))

//---------------------------------------------------------------------------
//
//	定数宣言(GIC)
//
//---------------------------------------------------------------------------
#define ARM_GICD_BASE		0xFF841000
#define ARM_GICC_BASE		0xFF842000
#define ARM_GIC_END			0xFF847FFF
#define GICD_CTLR			0x000
#define GICD_IGROUPR0		0x020
#define GICD_ISENABLER0		0x040
#define GICD_ICENABLER0		0x060
#define GICD_ISPENDR0		0x080
#define GICD_ICPENDR0		0x0A0
#define GICD_ISACTIVER0		0x0C0
#define GICD_ICACTIVER0		0x0E0
#define GICD_IPRIORITYR0	0x100
#define GICD_ITARGETSR0		0x200
#define GICD_ICFGR0			0x300
#define GICD_SGIR			0x3C0
#define GICC_CTLR			0x000
#define GICC_PMR			0x001
#define GICC_IAR			0x003
#define GICC_EOIR			0x004

//---------------------------------------------------------------------------
//
//	定数宣言(GIC IRQ)
//
//---------------------------------------------------------------------------
#define GIC_IRQLOCAL0		(16 + 14)
#define GIC_GPIO_IRQ		(32 + 116)	// GPIO3

//---------------------------------------------------------------------------
//
//	定数宣言(制御信号)
//
//---------------------------------------------------------------------------
#define ACT_OFF	!ACT_ON
#define ENB_OFF	!ENB_ON
#define TAD_OUT	!TAD_IN
#define IND_OUT	!IND_IN
#define DTD_OUT	!DTD_IN

//---------------------------------------------------------------------------
//
//	定数宣言(SCSI)
//
//---------------------------------------------------------------------------
#define IN		GPIO_INPUT
#define OUT		GPIO_OUTPUT
#define ON		TRUE
#define OFF		FALSE

//---------------------------------------------------------------------------
//
//	定数宣言(バス制御タイミング)
//
//---------------------------------------------------------------------------
#define GPIO_DATA_SETTLING 100			// データバスが安定する時間(ns)

//---------------------------------------------------------------------------
//
//	クラス定義
//
//---------------------------------------------------------------------------
class GPIOBUS : public BUS
{
public:
	// 基本ファンクション
	GPIOBUS();
										// コンストラクタ
	virtual ~GPIOBUS();
										// デストラクタ
	BOOL FASTCALL Init(mode_e mode = TARGET);
										// 初期化
	void FASTCALL Reset();
										// リセット
	void FASTCALL Cleanup();
										// クリーンアップ

	DWORD FASTCALL Aquire();
										// 信号取り込み

	void FASTCALL SetENB(BOOL ast);
										// ENBシグナル設定

	BOOL FASTCALL GetBSY();
										// BSYシグナル取得
	void FASTCALL SetBSY(BOOL ast);
										// BSYシグナル設定

	BOOL FASTCALL GetSEL();
										// SELシグナル取得
	void FASTCALL SetSEL(BOOL ast);
										// SELシグナル設定

	BOOL FASTCALL GetATN();
										// ATNシグナル取得
	void FASTCALL SetATN(BOOL ast);
										// ATNシグナル設定

	BOOL FASTCALL GetACK();
										// ACKシグナル取得
	void FASTCALL SetACK(BOOL ast);
										// ACKシグナル設定

	BOOL FASTCALL GetRST();
										// RSTシグナル取得
	void FASTCALL SetRST(BOOL ast);
										// RSTシグナル設定

	BOOL FASTCALL GetMSG();
										// MSGシグナル取得
	void FASTCALL SetMSG(BOOL ast);
										// MSGシグナル設定

	BOOL FASTCALL GetCD();
										// CDシグナル取得
	void FASTCALL SetCD(BOOL ast);
										// CDシグナル設定

	BOOL FASTCALL GetIO();
										// IOシグナル取得
	void FASTCALL SetIO(BOOL ast);
										// IOシグナル設定

	BOOL FASTCALL GetREQ();
										// REQシグナル取得
	void FASTCALL SetREQ(BOOL ast);
										// REQシグナル設定

	BYTE FASTCALL GetDAT();
										// データシグナル取得
	void FASTCALL SetDAT(BYTE dat);
										// データシグナル設定
	BOOL FASTCALL GetDP();
										// パリティシグナル取得
	int FASTCALL CommandHandShake(BYTE *buf);
										// コマンド受信ハンドシェイク
	int FASTCALL ReceiveHandShake(BYTE *buf, int count);
										// データ受信ハンドシェイク
	int FASTCALL SendHandShake(BYTE *buf, int count);
										// データ送信ハンドシェイク

#ifdef USE_SEL_EVENT_ENABLE
	// SEL信号割り込み関係
	int FASTCALL PollSelectEvent();
										// SEL信号イベントポーリング
	void FASTCALL ClearSelectEvent();
										// SEL信号イベントクリア
#endif	// USE_SEL_EVENT_ENABLE

private:
	// SCSI入出力信号制御
	void FASTCALL MakeTable();
										// ワークテーブル作成
	void FASTCALL SetControl(int pin, BOOL ast);
										// 制御信号設定
	void FASTCALL SetMode(int pin, int mode);
										// SCSI入出力モード設定
	BOOL FASTCALL GetSignal(int pin);
										// SCSI入力信号値取得
	void FASTCALL SetSignal(int pin, BOOL ast);
										// SCSI出力信号値設定
	BOOL FASTCALL WaitSignal(int pin, BOOL ast);
										// 信号変化待ち
	// 割り込み制御
	void FASTCALL DisableIRQ();
										// IRQ禁止
	void FASTCALL EnableIRQ();
										// IRQ有効

	//  GPIOピン機能設定
	void FASTCALL PinConfig(int pin, int mode);
										// GPIOピン機能設定(入出力設定)
	void FASTCALL PullConfig(int pin, int mode);
										// GPIOピン機能設定(プルアップ/ダウン)
	void FASTCALL PinSetSignal(int pin, BOOL ast);
										// GPIOピン出力信号設定
	void FASTCALL DrvConfig(DWORD drive);
										// GPIOドライブ能力設定


	mode_e actmode;						// 動作モード

	DWORD baseaddr;						// ベースアドレス

	int rpitype;
										// ラズパイ種別

	volatile DWORD *gpio;				// GPIOレジスタ

	volatile DWORD *pads;				// PADSレジスタ

	volatile DWORD *level;				// GPIO入力レベル

	volatile DWORD *irpctl;				// 割り込み制御レジスタ

#ifndef BAREMETAL
	volatile DWORD irptenb;				// 割り込み有効状態

	volatile DWORD *qa7regs;			// QA7レジスタ

	volatile int tintcore;				// 割り込み制御対象CPU

	volatile DWORD tintctl;				// 割り込みコントロール

	volatile DWORD giccpmr;				// GICC 優先度設定
#endif	// BAREMETAL

	volatile DWORD *gicd;				// GIC 割り込み分配器レジスタ

	volatile DWORD *gicc;				// GIC CPUインターフェースレジスタ

	DWORD gpfsel[4];					// GPFSEL0-4バックアップ

	DWORD signals;						// バス全信号

#if defined(USE_SEL_EVENT_ENABLE) && !defined(BAREMETAL)
	struct gpioevent_request selevreq;	// SEL信号イベント要求

	int epfd;							// epollファイルディスクプリタ
#endif	// USE_SEL_EVENT_ENABLE && !BAREMETAL

#if SIGNAL_CONTROL_MODE == 0
	DWORD tblDatMsk[3][256];			// データマスク用テーブル

	DWORD tblDatSet[3][256];			// データ設定用テーブル
#else
	DWORD tblDatMsk[256];				// データマスク用テーブル

	DWORD tblDatSet[256];				// データ設定用テーブル
#endif

	static const int SignalTable[19];	// シグナルテーブル
};

//===========================================================================
//
//	システムタイマ
//
//===========================================================================
class SysTimer
{
public:
	static void FASTCALL Init(DWORD *syst, DWORD *armt);
										// 初期化
	static DWORD FASTCALL GetTimerLow();
										// システムタイマ(LO)取得
	static DWORD FASTCALL GetTimerHigh();
										// システムタイマ(HI)取得
	static void FASTCALL SleepNsec(DWORD nsec);
										// ナノ秒単位のスリープ
	static void FASTCALL SleepUsec(DWORD usec);
										// μ秒単位のスリープ

private:
	static volatile DWORD *systaddr;
										// システムタイマアドレス
	static volatile DWORD *armtaddr;
										// ARMタイマアドレス
	static volatile DWORD corefreq;
										// コア周波数
};

#endif	// gpiobus_h
