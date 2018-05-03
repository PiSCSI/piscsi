//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2018 GIMONS
//	[ GPIOドライバ ]
//
//---------------------------------------------------------------------------

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/uaccess.h>

//---------------------------------------------------------------------------
//
//	基本型定義
//
//---------------------------------------------------------------------------
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef int BOOL;

#if !defined(FALSE)
#define FALSE               0
#endif

#if !defined(TRUE)
#define TRUE                1
#endif

//---------------------------------------------------------------------------
//
//	接続方法定義の選択
//
//---------------------------------------------------------------------------
#define CONNECT_TYPE_STANDARD		// 標準(SCSI論理,標準ピンアサイン)
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
#define GPIO_OFFSET		0x200000
#define PADS_OFFSET		0x100000
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
#define PAD_0_27		11

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
//	定数宣言(ドライバ)
//
//---------------------------------------------------------------------------
#define DEVICE_NAME 	"rascsidrv"
#define DRIVER_PATH		"/dev/" DEVICE_NAME
#define IOCTL_INIT		0x100
#define IOCTL_MODE		0x101
#define IOCTL_PADS		0x102

//---------------------------------------------------------------------------
//
//	動作モード
//
//---------------------------------------------------------------------------
enum mode_e {
	TARGET = 0,
	INITIATOR = 1,
	MONITOR = 2,
};

//---------------------------------------------------------------------------
//
//	ドライバ関数宣言
//
//---------------------------------------------------------------------------
static int drv_init(void);
static void drv_exit(void);

//---------------------------------------------------------------------------
//
//	エントリポイント宣言
//
//---------------------------------------------------------------------------
int drv_open(struct inode *inode, struct file *file);
int drv_close(struct inode *inode, struct file *file);
long drv_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
ssize_t drv_read(
	struct file *file, char __user *buf, size_t count, loff_t *fops);
ssize_t drv_write(
	struct file *file, const char __user *buf, size_t count, loff_t *fops);
struct file_operations drv_fops = {
  .open = drv_open,
  .release = drv_close,
  .unlocked_ioctl = drv_ioctl,
  .read = drv_read,
  .write = drv_write,
};

//---------------------------------------------------------------------------
//
//	変数宣言(ドライバ登録)
//
//---------------------------------------------------------------------------
static dev_t drv_dev;
static dev_t drv_devno;
static struct cdev drv_cdev;
static struct class *drv_class;

//---------------------------------------------------------------------------
//
//	変数宣言(GPIO制御)
//
//---------------------------------------------------------------------------
static DWORD baseaddr;					// BASEアドレス
static volatile DWORD *gpio;			// GPIOレジスタ
static volatile DWORD *level;			// GPIOレジスタ(レベル)
static volatile DWORD *pads;			// PADSレジスタ

#if SIGNAL_CONTROL_MODE == 0
static DWORD gpfsel[4];					// FSELバックアップ
#endif

//---------------------------------------------------------------------------
//
//	変数宣言(SCSI制御)
//
//---------------------------------------------------------------------------
static enum mode_e actmode;				// 動作モード
static DWORD signals;					// SCSI全信号

static BOOL tblParity[256];
#if SIGNAL_CONTROL_MODE == 0
static DWORD tblDatMsk[3][256];			// データマスク用テーブル
static DWORD tblDatSet[3][256];			// データ設定用テーブル
#else
static DWORD tblDatMsk[256];			// データマスク用テーブル
static DWORD tblDatSet[256];			// データ設定用テーブル
#endif

//---------------------------------------------------------------------------
//
//	ワークテーブル作成
//
//---------------------------------------------------------------------------
void MakeTable(void)
{
	const int pintbl[] = {
		PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4,
		PIN_DT5, PIN_DT6, PIN_DT7, PIN_DP
	};

	int i;
	int j;
	DWORD bits;
	DWORD parity;
#if SIGNAL_CONTROL_MODE == 0
	int index;
	int shift;
#else
	DWORD gpclr;
	DWORD gpset;
#endif
	
	// パリティテーブル作成
	for (i = 0; i < 0x100; i++) {
		bits = (DWORD)i;
		parity = 0;
		for (j = 0; j < 8; j++) {
			parity ^= bits & 1;
			bits >>= 1;
		}
		parity = ~parity;
		tblParity[i] = parity & 1;
	}

#if SIGNAL_CONTROL_MODE == 0
	// マスクと設定データ生成
	memset(tblDatMsk, 0xff, sizeof(tblDatMsk));
	memset(tblDatSet, 0x00, sizeof(tblDatSet));
	for (i = 0; i < 0x100; i++) {
		// 検査用ビット列
		bits = (DWORD)i;

		// パリティ取得
		if (tblParity[i]) {
			bits |= (1 << 8);
		}

		// ビット検査
		for (j = 0; j < 9; j++) {
			// インデックスとシフト量計算
			index = pintbl[j] / 10;
			shift = (pintbl[j] % 10) * 3;

			// マスクデータ
			tblDatMsk[index][i] &= ~(0x7 << shift);

			// 設定データ
			if (bits & 1) {
				tblDatSet[index][i] |= (1 << shift);
			}

			bits >>= 1;
		}
	}
#else
	// マスクと設定データ生成
	memset(tblDatMsk, 0x00, sizeof(tblDatMsk));
	memset(tblDatSet, 0x00, sizeof(tblDatSet));
	for (i = 0; i < 0x100; i++) {
		// 検査用ビット列
		bits = (DWORD)i;

		// パリティ取得
		if (tblParity[i]) {
			bits |= (1 << 8);
		}

#if SIGNAL_CONTROL_MODE == 1
		// 負論理は反転
		bits = ~bits;
#endif

		// GPIOレジスタ情報の作成
		gpclr = 0;
		gpset = 0;
		for (j = 0; j < 9; j++) {
			if (bits & 1) {
				gpset |= (1 << pintbl[j]);
			} else {
				gpclr |= (1 << pintbl[j]);
			}
			bits >>= 1;
		}

		tblDatMsk[i] = gpclr;
		tblDatSet[i] = gpset;
	}
#endif
}

//---------------------------------------------------------------------------
//
//	ドライバ初期化
//
//---------------------------------------------------------------------------
static int drv_init(void)
{
	printk(KERN_INFO "RaSCSI GPIO Driver Loaded(%s)\n", CONNECT_DESC);

	// デバイス番号取得
	if (alloc_chrdev_region(
		&drv_dev,
		0,
		1,
		DEVICE_NAME) < 0) {
		printk(KERN_DEBUG "Failed to allocate region\n");
		goto init_error;
	}

	// クラス登録
	if ((drv_class = class_create(
		THIS_MODULE,
		DEVICE_NAME)) == NULL) {
		printk(KERN_DEBUG "Failed to create class\n");
		goto init_error;
	}

	// デバイス初期化
	cdev_init(&drv_cdev, &drv_fops);
	drv_cdev.owner = THIS_MODULE;
	if (cdev_add(&drv_cdev, drv_dev, 1) < 0) {
		printk(KERN_ALERT "Failed to add device\n");
		goto init_error;
	}

	// デバイス作成
	drv_devno = MKDEV(MAJOR(drv_dev), MINOR(drv_dev));
	if (device_create(
		drv_class,
		NULL,
		drv_devno,
		NULL,
		DEVICE_NAME) == NULL) {
		printk(KERN_DEBUG "Failed to create device\n");
		goto init_error;
	}

	// ワークテーブル生成
	MakeTable();

	// ワーククリア
	baseaddr = ~0;
	gpio = NULL;
	level = NULL;
	pads = NULL;

	return 0;

init_error:
	drv_exit();
	return -1;
}

//---------------------------------------------------------------------------
//
//	ドライバ解放
//
//---------------------------------------------------------------------------
static void drv_exit(void)
{
	// クラス登録解除
	if (drv_class) {
		device_destroy(drv_class, drv_devno);
		class_destroy(drv_class);
		drv_class = NULL;
	}
	
	// デバイス登録解除
	if (drv_dev){
		unregister_chrdev_region(drv_dev, 1);
		drv_dev = 0;
	}
	
	printk(KERN_INFO "RaSCSI GPIO Driver Unloaded\n");
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
int drv_open(struct inode *inode, struct file *file)
{
	// 動作モード初期化
	actmode = TARGET;

	return 0;
}

//---------------------------------------------------------------------------
//
//	クローズ
//
//---------------------------------------------------------------------------
int drv_close(struct inode *inode, struct file *file)
{
	// アンマップ
	if (gpio) {
		iounmap(gpio);
		gpio = NULL;
	}

	if (pads) {
		iounmap(pads);
		pads = NULL;
	}

	// ベースアドレスクリア
	baseaddr = ~0;

	return 0;
}

//---------------------------------------------------------------------------
//
//	ドライバ IOCTL
//
//---------------------------------------------------------------------------
long drv_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	DWORD data;
	DWORD drive;

	switch (cmd) {
		case IOCTL_INIT:
			// 初期化はオープン後に1回限り
			if (baseaddr != ~0) {
				break;
			}

			// ベースアドレス
			baseaddr = arg;

			// GPIOレジスタをマップ
			gpio = (DWORD *)ioremap_nocache(baseaddr + GPIO_OFFSET, 164);
			level = &gpio[GPIO_LEV_0];

			// PADSレジスタをマップ
			pads = (DWORD *)ioremap_nocache(baseaddr + PADS_OFFSET, 56);
			break;

		case IOCTL_MODE:
			// 動作モード変更
			actmode = (enum mode_e)arg;
			break;

		case IOCTL_PADS:
			// DriveStrength変更
			if (!pads) {
				return -1;
			}

			data = pads[PAD_0_27];
			drive = arg;
			pads[PAD_0_27] = (0xFFFFFFF8 & data) | drive | 0x5a000000;
			break;

		default:
			break;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	バス信号取り込み
//
//---------------------------------------------------------------------------
static inline DWORD Aquire(void)
{
	signals = *level;

#if SIGNAL_CONTROL_MODE < 2
	// 負論理なら反転する(内部処理は正論理に統一)
	signals = ~signals;
#endif
	
	return signals;
}

//---------------------------------------------------------------------------
//
//	ナノ秒単位のスリープ
//
//	GPIO関係のレジスタはコアクロックが400の時に計測した速度を参考にした。
//	リード:48ns
//	ライト: 8ns
//
//---------------------------------------------------------------------------
static inline void SleepNsec(DWORD nsec)
{
	DWORD count;

	count = (nsec + 8 - 1) / 8;
	while (count--) {
		gpio[GPIO_PUD] = 0;
	}
}

//---------------------------------------------------------------------------
//
//	シグナル設定
//
//---------------------------------------------------------------------------
static inline void SetSignal(int pin, BOOL ast)
{
#if SIGNAL_CONTROL_MODE == 0
	int index;
	int shift;
	DWORD data;

	index = pin / 10;
	shift = (pin % 10) * 3;
	data = gpfsel[index];
	data &= ~(0x7 << shift);
	if (ast) {
		data |= (1 << shift);
	}
	gpio[GPIO_FSEL_0 + index] = data;
	gpfsel[index] = data;
#elif SIGNAL_CONTROL_MODE == 1
	if (ast) {
		gpio[GPIO_CLR_0] = 0x1 << pin;
	} else {
		gpio[GPIO_SET_0] = 0x1 << pin;
	}
#elif SIGNAL_CONTROL_MODE == 2
	if (ast) {
		gpio[GPIO_SET_0] = 0x1 << pin;
	} else {
		gpio[GPIO_CLR_0] = 0x1 << pin;
	}
#endif
}

//---------------------------------------------------------------------------
//
//	データ取得
//
//---------------------------------------------------------------------------
static inline BYTE GetDAT(void)
{
	DWORD data;

	data = Aquire();
	data =
		((data >> (PIN_DT0 - 0)) & (1 << 0)) |
		((data >> (PIN_DT1 - 1)) & (1 << 1)) |
		((data >> (PIN_DT2 - 2)) & (1 << 2)) |
		((data >> (PIN_DT3 - 3)) & (1 << 3)) |
		((data >> (PIN_DT4 - 4)) & (1 << 4)) |
		((data >> (PIN_DT5 - 5)) & (1 << 5)) |
		((data >> (PIN_DT6 - 6)) & (1 << 6)) |
		((data >> (PIN_DT7 - 7)) & (1 << 7));
		
	return (BYTE)data;
}

//---------------------------------------------------------------------------
//
//	データ設定
//
//---------------------------------------------------------------------------
static inline void SetDAT(BYTE dat)
{
	// ポートへ書き込み
#if SIGNAL_CONTROL_MODE == 0
	gpfsel[0] &= tblDatMsk[0][dat];
	gpfsel[0] |= tblDatSet[0][dat];
	gpio[GPIO_FSEL_0] = gpfsel[0];

	gpfsel[1] &= tblDatMsk[1][dat];
	gpfsel[1] |= tblDatSet[1][dat];
	gpio[GPIO_FSEL_1] = gpfsel[1];

	gpfsel[2] &= tblDatMsk[2][dat];
	gpfsel[2] |= tblDatSet[2][dat];
	gpio[GPIO_FSEL_2] = gpfsel[2];
#else
	gpio[GPIO_CLR_0] = tblDatMsk[dat];
	gpio[GPIO_SET_0] = tblDatSet[dat];
#endif
}

//---------------------------------------------------------------------------
//
//	リード
//
//---------------------------------------------------------------------------
ssize_t drv_read(
	struct file *file, char __user *buf, size_t count, loff_t *fops)
{
	ssize_t i;
	BYTE data;
	DWORD loop;
	DWORD phase;

#if SIGNAL_CONTROL_MODE == 0
	// FSEL事前取得
	gpfsel[0] = gpio[GPIO_FSEL_0];
	gpfsel[1] = gpio[GPIO_FSEL_1];
	gpfsel[2] = gpio[GPIO_FSEL_2];
#endif

	// 割り込み禁止
	local_irq_disable();
	local_fiq_disable();

	if (actmode == TARGET) {
		for (i = 0; i < count; i++) {
			// REQアサート
			SetSignal(PIN_REQ, TRUE);

			// ACKアサート待ち
			loop = (DWORD)30000000;
			do {
				Aquire();
				loop--;
			} while (!(signals & (1 << PIN_ACK)) &&
				!(signals & (1 << PIN_RST)) && loop > 0);

			// データ取得
			data = GetDAT();

			// REQネゲート
			SetSignal(PIN_REQ, FALSE);

			// ACKアサート待ちでタイムアウト
			if (loop == 0 || (signals & (1 << PIN_RST))) {
				break;
			}

			// ACKネゲート待ち
			loop = (DWORD)30000000;
			do {
				Aquire();
				loop--;
			} while ((signals & (1 << PIN_ACK)) &&
				!(signals & (1 << PIN_RST)) && loop > 0);

			// ACKネゲート待ちでタイムアウト
			if (loop == 0 || (signals & (1 << PIN_RST))) {
				break;
			}

			// データ返却
			if (copy_to_user(buf, (unsigned char *)&data, 1)) {
				break;
			}

			// 次データへ
			buf++;
		}
	} else {
		// フェーズ取得
		Aquire();
		phase = signals & GPIO_MCI;

		for (i = 0; i < count; i++) {
			// REQアサート待ち
			loop = 30000000;
			do {
				Aquire();
				loop--;
			} while (!(signals & (1 << PIN_REQ)) &&
				((signals & GPIO_MCI) == phase) && loop > 0);

			// REQアサート待ちでタイムアウト
			if (loop == 0 || (signals & GPIO_MCI) != phase) {
				break;
			}

			// データ取得
			data = GetDAT();

			// ACKアサート
			SetSignal(PIN_ACK, TRUE);

			// REQネゲート待ち
			loop = 30000000;
			do {
				Aquire();
				loop--;
			} while ((signals & (1 << PIN_REQ)) &&
				((signals & GPIO_MCI) == phase) && loop > 0);

			// ACKネゲート
			SetSignal(PIN_ACK, FALSE);

			// REQネゲート待ちでタイムアウト
			if (loop == 0 || (signals & GPIO_MCI) != phase) {
				break;
			}

			// データ返却
			if (copy_to_user(buf, (unsigned char *)&data, 1)) {
				break;
			}

			// 次データへ
			buf++;
		}
	}

	// 割り込み禁止解除
	local_fiq_enable();
	local_irq_enable();

	return i;
}

//---------------------------------------------------------------------------
//
//	ライト
//
//---------------------------------------------------------------------------
ssize_t drv_write(
	struct file *file, const char __user *buf, size_t count, loff_t *fops)
{
	size_t i;
	BYTE data;
	DWORD loop;
	DWORD phase;

	// 割り込み禁止
	local_irq_disable();
	local_fiq_disable();

#if SIGNAL_CONTROL_MODE == 0
	// FSEL事前取得
	gpfsel[0] = gpio[GPIO_FSEL_0];
	gpfsel[1] = gpio[GPIO_FSEL_1];
	gpfsel[2] = gpio[GPIO_FSEL_2];
#endif

	if (actmode == TARGET) {
		for (i = 0; i < count; i++) {
			// データ取り込み
			if (copy_from_user((unsigned char *)&data, buf, 1)) {
				break;
			}

			// データ設定
			SetDAT(data);

			// 信号線が安定するまでウェイト
			SleepNsec(150);

			// ACKネゲート待ち
			loop = (DWORD)30000000;
			do {
				Aquire();
				loop--;
			} while ((signals & (1 << PIN_ACK)) &&
				!(signals & (1 << PIN_RST)) && loop > 0);

			// ACKネゲート待ちでタイムアウト
			if (loop == 0 || (signals & (1 << PIN_RST))) {
				break;
			}

			// REQアサート
			SetSignal(PIN_REQ, TRUE);

			// ACKアサート待ち
			loop = (DWORD)30000000;
			do {
				Aquire();
				loop--;
			} while (!(signals & (1 << PIN_ACK)) &&
				!(signals & (1 << PIN_RST)) && loop > 0);

			// REQネゲート
			SetSignal(PIN_REQ, FALSE);

			// ACKアサート待ちでタイムアウト
			if (loop == 0 || (signals & (1 << PIN_RST))) {
				break;
			}

			// 次データ
			buf++;
		};

		// ACKネゲート待ち
		loop = (DWORD)30000000;
		do {
			Aquire();
			loop--;
		} while ((signals & (1 << PIN_ACK)) &&
			!(signals & (1 << PIN_RST)) && loop > 0);
	} else {
		// フェーズ取得
		Aquire();
		phase = signals & GPIO_MCI;

		for (i = 0; i < count; i++) {
			// データ取り込み
			if (copy_from_user((unsigned char *)&data, buf, 1)) {
				break;
			}

			// REQアサート待ち
			loop = (DWORD)30000000;
			do {
				Aquire();
				loop--;
			} while (!(signals & (1 << PIN_REQ)) &&
				((signals & GPIO_MCI) == phase) && loop > 0);

			// REQアサート待ちでタイムアウト
			if (loop == 0 || (signals & GPIO_MCI) != phase) {
				break;
			}

			// データ設定
			SetDAT(data);

			// 信号線が安定するまでウェイト
			SleepNsec(150);

			// ACKアサート
			SetSignal(PIN_ACK, TRUE);

			// REQネゲート待ち
			loop = (DWORD)30000000;
			do {
				Aquire();
				loop--;
			} while ((signals & (1 << PIN_REQ)) &&
				((signals & GPIO_MCI) == phase) && loop > 0);

			// ACKネゲート
			SetSignal(PIN_ACK, FALSE);

			// REQネゲート待ちでタイムアウト
			if (loop == 0 || (signals & GPIO_MCI) != phase) {
				break;
			}

			// 次データへ
			buf++;
		}
	}

	// 割り込み禁止解除
	local_fiq_enable();
	local_irq_enable();

	return i;
}

module_init(drv_init);
module_exit(drv_exit);

MODULE_DESCRIPTION("RASCSI GPIO Driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("GIMONS");
MODULE_LICENSE("GPL");
