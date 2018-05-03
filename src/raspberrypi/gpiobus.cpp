//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2018 GIMONS
//
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
//
//	[ GPIO-SCSIバス ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "gpiobus.h"

#ifdef __linux__
//---------------------------------------------------------------------------
//
//	imported from bcm_host.c
//
//---------------------------------------------------------------------------
static DWORD get_dt_ranges(const char *filename, DWORD offset)
{
	DWORD address;
	FILE *fp;
	BYTE buf[4];

	address = ~0;
	fp = fopen(filename, "rb");
	if (fp) {
		fseek(fp, offset, SEEK_SET);
		if (fread(buf, 1, sizeof buf, fp) == sizeof buf) {
			address =
				buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0;
		}
		fclose(fp);
	}
	return address;
}

DWORD bcm_host_get_peripheral_address(void)
{
	DWORD address;
	
	address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
	return address == (DWORD)~0 ? 0x20000000 : address;
}
#endif // __linux__

#ifdef __NetBSD__
// Raspberry Piシリーズを仮定してCPUからアドレスを推定
DWORD bcm_host_get_peripheral_address(void)
{
	char buf[1024];
	size_t len = sizeof(buf);
	DWORD address;
	
	if (sysctlbyname("hw.model", buf, &len, NULL, 0) ||
	    strstr(buf, "ARM1176JZ-S") != buf) {
		// CPUモデルの取得に失敗 || BCM2835ではない
		// BCM283[67]のアドレスを使用
		address = 0x3f000000;
	} else {
		// BCM2835のアドレスを使用
		address = 0x20000000;
	}
	printf("Peripheral address : 0x%lx\n", address);
	return address;
}
#endif

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
GPIOBUS::GPIOBUS()
{
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
GPIOBUS::~GPIOBUS()
{
}

//---------------------------------------------------------------------------
//
//	ピン機能設定(入出力設定)
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::PinConfig(int pin, int mode)
{
	int index;
	DWORD mask;

	// 未使用なら無効
	if (pin < 0) {
		return;
	}

	index = pin / 10;
	mask = ~(0x7 << ((pin % 10) * 3));
	gpio[index] = (gpio[index] & mask) | ((mode & 0x7) << ((pin % 10) * 3));
}

//---------------------------------------------------------------------------
//
//	ピン機能設定(プルアップ/ダウン)
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::PullConfig(int pin, int mode)
{
	// 未使用なら無効
	if (pin < 0) {
		return;
	}

	gpio[GPIO_PUD] = mode & 0x3;
	usleep(1);
	gpio[GPIO_CLK_0] = 0x1 << pin;
	usleep(1);
	gpio[GPIO_PUD] = 0;
	gpio[GPIO_CLK_0] = 0;
}

//---------------------------------------------------------------------------
//
//	Drive Strength設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::DrvConfig(DWORD drive)
{
	DWORD data;

	// カーネルドライバがあればIOCTLで依頼する
	if (drvfd >= 0) {
		ioctl(drvfd, IOCTL_PADS, (DWORD)drive);
	} else {
		data = pads[PAD_0_27];
		pads[PAD_0_27] = (0xFFFFFFF8 & data) | drive | 0x5a000000;
	}
}

//---------------------------------------------------------------------------
//
//	ピン出力設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::PinSetSignal(int pin, BOOL ast)
{
	// 未使用なら無効
	if (pin < 0) {
		return;
	}

	if (ast) {
		gpio[GPIO_SET_0] = 0x1 << pin;
	} else {
		gpio[GPIO_CLR_0] = 0x1 << pin;
	}
}

//---------------------------------------------------------------------------
//
//	ワークテーブル作成
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::MakeTable(void)
{
	const int pintbl[] = {
		PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4,
		PIN_DT5, PIN_DT6, PIN_DT7, PIN_DP
	};

	int i;
	int j;
	BOOL tblParity[256];
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
//	初期化
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::Init(mode_e mode)
{
	DWORD base;
	DWORD addr;
	int fd;
	void *map;
	int i;
	int j;
	int pullmode;

	// 動作モードの保存
	actmode = mode;

	// ベースアドレスの取得
	base = (DWORD)bcm_host_get_peripheral_address();

	// カーネルドライバオープン
	drvfd = open(DRIVER_PATH, O_RDWR);
	if (drvfd >= 0) {
		printf("Activated RaSCSI GPIO Driver.\n");
		ioctl(drvfd, IOCTL_INIT, base);
		ioctl(drvfd, IOCTL_MODE, (DWORD)actmode);
	}

	// /dev/mem,/dev/gpiomemの順にオープンを試みる
	addr = base + GPIO_OFFSET;
	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd == -1) {
		// /dev/gpiomemをオープン
		addr = 0;
		fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
		if (fd == -1) {
			return FALSE;
		}
	}

	// GPIOのI/Oポートをマップする
	map = mmap(NULL, 164,
		PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr);
	if (map == MAP_FAILED) {
		close(fd);
		return FALSE;
	}

	// GPIOのI/Oポートアドレス確保
	gpio = (DWORD *)map;
	level = &gpio[GPIO_LEV_0];

	// PADSの処理はカーネルドライバが無い時だけ(sudoが必要となる)
	if (drvfd < 0) {
		// /dev/memをオープン
		fd = open("/dev/mem", O_RDWR | O_SYNC);
		if (fd == -1) {
			return FALSE;
		}

		// PADSのI/Oポートをマップする
		map = mmap(NULL, 56,
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, base + PADS_OFFSET);
		close(fd);
		if (map == MAP_FAILED) {
			return FALSE;
		}

		// PADSのI/Oポートアドレス確保
		pads = (DWORD *)map;
	}

	// Drive Strengthを16mAに設定
	DrvConfig(7);

	// プルアップ/プルダウンを設定
#if SIGNAL_CONTROL_MODE == 0
	pullmode = GPIO_PULLNONE;
#elif SIGNAL_CONTROL_MODE == 1
	pullmode = GPIO_PULLUP;
#else
	pullmode = GPIO_PULLDOWN;
#endif

	// 全信号初期化
	for (i = 0; SignalTable[i] >= 0; i++) {
		j = SignalTable[i];
		PinSetSignal(j, FALSE);
		PinConfig(j, GPIO_INPUT);
		PullConfig(j, pullmode);
	}

	// 制御信号を設定
	PinSetSignal(PIN_ACT, FALSE);
	PinSetSignal(PIN_TAD, FALSE);
	PinSetSignal(PIN_IND, FALSE);
	PinSetSignal(PIN_DTD, FALSE);
	PinConfig(PIN_ACT, GPIO_OUTPUT);
	PinConfig(PIN_TAD, GPIO_OUTPUT);
	PinConfig(PIN_IND, GPIO_OUTPUT);
	PinConfig(PIN_DTD, GPIO_OUTPUT);

	// 有効信号を設定
	PinSetSignal(PIN_ENB, ENB_ON);
	PinConfig(PIN_ENB, GPIO_OUTPUT);

	// GPFSELバックアップ
	gpfsel[0] = gpio[GPIO_FSEL_0];
	gpfsel[1] = gpio[GPIO_FSEL_1];
	gpfsel[2] = gpio[GPIO_FSEL_2];
	gpfsel[3] = gpio[GPIO_FSEL_3];

	// ワークテーブル作成
	MakeTable();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	クリーンアップ
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::Cleanup()
{
	int i;
	int j;

	// 制御信号を設定
	PinSetSignal(PIN_ENB, FALSE);
	PinSetSignal(PIN_ACT, FALSE);
	PinSetSignal(PIN_TAD, FALSE);
	PinSetSignal(PIN_IND, FALSE);
	PinSetSignal(PIN_DTD, FALSE);
	PinConfig(PIN_ACT, GPIO_INPUT);
	PinConfig(PIN_TAD, GPIO_INPUT);
	PinConfig(PIN_IND, GPIO_INPUT);
	PinConfig(PIN_DTD, GPIO_INPUT);

	// 全信号初期化
	for (i = 0; SignalTable[i] >= 0; i++) {
		j = SignalTable[i];
		PinSetSignal(j, FALSE);
		PinConfig(j, GPIO_INPUT);
		PullConfig(j, GPIO_PULLNONE);
	}

	// Drive Strengthを8mAに設定
	DrvConfig(3);

	// カーネルドライバ
	if (drvfd >= 0) {
		close(drvfd);
		drvfd = -1;
	}
}

//---------------------------------------------------------------------------
//
//	制御信号設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetControl(int pin, BOOL ast)
{
	PinSetSignal(pin, ast);
}

//---------------------------------------------------------------------------
//
//	入出力モード設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetMode(int pin, int mode)
{
	int index;
	int shift;
	DWORD data;

#if SIGNAL_CONTROL_MODE == 0
	if (mode == OUT) {
		return;
	}
#endif

	index = pin / 10;
	shift = (pin % 10) * 3;
	data = gpfsel[index];
	data &= ~(0x7 << shift);
	if (mode == OUT) {
		data |= (1 << shift);
	}
	gpio[index] = data;
	gpfsel[index] = data;
}
	
//---------------------------------------------------------------------------
//
//	入力信号値取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetSignal(int pin)
{
	return  (signals >> pin) & 1;
}
	
//---------------------------------------------------------------------------
//
//	出力信号値設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetSignal(int pin, BOOL ast)
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
	gpio[index] = data;
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
//	ナノ秒単位のスリープ
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SleepNsec(DWORD nsec)
{
	DWORD count;

	count = (nsec + 8 - 1) / 8;
	while (count--) {
		gpio[GPIO_PUD] = 0;
	}
}

//---------------------------------------------------------------------------
//
//	リセット
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::Reset()
{
	int i;
	int j;

	// アクティブ信号をオフ
	SetControl(PIN_ACT, ACT_OFF);

	// 全信号をネゲートに設定
	for (i = 0;; i++) {
		j = SignalTable[i];
		if (j < 0) {
			break;
		}

		SetSignal(j, OFF);
	}

	if (actmode == TARGET) {
		// ターゲットモード

		// ターゲット信号を入力に設定
		SetControl(PIN_TAD, TAD_IN);
		SetMode(PIN_BSY, IN);
		SetMode(PIN_MSG, IN);
		SetMode(PIN_CD, IN);
		SetMode(PIN_REQ, IN);
		SetMode(PIN_IO, IN);

		// イニシエータ信号を入力に設定
		SetControl(PIN_IND, IND_IN);
		SetMode(PIN_SEL, IN);
		SetMode(PIN_ATN, IN);
		SetMode(PIN_ACK, IN);
		SetMode(PIN_RST, IN);

		// データバス信号を入力に設定
		SetControl(PIN_DTD, DTD_IN);
		SetMode(PIN_DT0, IN);
		SetMode(PIN_DT1, IN);
		SetMode(PIN_DT2, IN);
		SetMode(PIN_DT3, IN);
		SetMode(PIN_DT4, IN);
		SetMode(PIN_DT5, IN);
		SetMode(PIN_DT6, IN);
		SetMode(PIN_DT7, IN);
		SetMode(PIN_DP, IN);
	} else {
		// イニシエータモード

		// ターゲット信号を入力に設定
		SetControl(PIN_TAD, TAD_IN);
		SetMode(PIN_BSY, IN);
		SetMode(PIN_MSG, IN);
		SetMode(PIN_CD, IN);
		SetMode(PIN_REQ, IN);
		SetMode(PIN_IO, IN);

		// イニシエータ信号を出力に設定
		SetControl(PIN_IND, IND_OUT);
		SetMode(PIN_SEL, OUT);
		SetMode(PIN_ATN, OUT);
		SetMode(PIN_ACK, OUT);
		SetMode(PIN_RST, OUT);

		// データバス信号を出力に設定
		SetControl(PIN_DTD, DTD_OUT);
		SetMode(PIN_DT0, OUT);
		SetMode(PIN_DT1, OUT);
		SetMode(PIN_DT2, OUT);
		SetMode(PIN_DT3, OUT);
		SetMode(PIN_DT4, OUT);
		SetMode(PIN_DT5, OUT);
		SetMode(PIN_DT6, OUT);
		SetMode(PIN_DT7, OUT);
		SetMode(PIN_DP, OUT);
	}

	// 全信号初期化
	signals = 0;
}

//---------------------------------------------------------------------------
//
//	バス信号取り込み
//
//---------------------------------------------------------------------------
DWORD FASTCALL GPIOBUS::Aquire()
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
//	BSYシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetBSY()
{
	return GetSignal(PIN_BSY);
}

//---------------------------------------------------------------------------
//
//	BSYシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetBSY(BOOL ast)
{
	if (actmode == TARGET) {
		if (ast) {
			// アクティブ信号をオン
			SetControl(PIN_ACT, ACT_ON);

			// ターゲット信号を出力に設定
			SetControl(PIN_TAD, TAD_OUT);
			SetMode(PIN_BSY, OUT);
			SetMode(PIN_MSG, OUT);
			SetMode(PIN_CD, OUT);
			SetMode(PIN_REQ, OUT);
			SetMode(PIN_IO, OUT);
		} else {
			// ターゲット信号を入力に設定
			SetControl(PIN_TAD, TAD_IN);
			SetMode(PIN_BSY, IN);
			SetMode(PIN_MSG, IN);
			SetMode(PIN_CD, IN);
			SetMode(PIN_REQ, IN);
			SetMode(PIN_IO, IN);

			// アクティブ信号をオフ
			SetControl(PIN_ACT, ACT_OFF);
		}
	}

	// BSY信号を設定
	SetSignal(PIN_BSY, ast);
}

//---------------------------------------------------------------------------
//
//	SELシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetSEL()
{
	return GetSignal(PIN_SEL);
}

//---------------------------------------------------------------------------
//
//	SELシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetSEL(BOOL ast)
{
	if (actmode == INITIATOR && ast) {
		// アクティブ信号をオン
		SetControl(PIN_ACT, ACT_ON);
	}

	// SEL信号を設定
	SetSignal(PIN_SEL, ast);
}

//---------------------------------------------------------------------------
//
//	ATNシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetATN()
{
	return GetSignal(PIN_ATN);
}

//---------------------------------------------------------------------------
//
//	ATNシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetATN(BOOL ast)
{
	SetSignal(PIN_ATN, ast);
}

//---------------------------------------------------------------------------
//
//	ACKシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetACK()
{
	return GetSignal(PIN_ACK);
}

//---------------------------------------------------------------------------
//
//	ACKシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetACK(BOOL ast)
{
	SetSignal(PIN_ACK, ast);
}

//---------------------------------------------------------------------------
//
//	RSTシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetRST()
{
	return GetSignal(PIN_RST);
}

//---------------------------------------------------------------------------
//
//	RSTシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetRST(BOOL ast)
{
	SetSignal(PIN_RST, ast);
}

//---------------------------------------------------------------------------
//
//	MSGシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetMSG()
{
	return GetSignal(PIN_MSG);
}

//---------------------------------------------------------------------------
//
//	MSGシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetMSG(BOOL ast)
{
	SetSignal(PIN_MSG, ast);
}

//---------------------------------------------------------------------------
//
//	CDシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetCD()
{
	return GetSignal(PIN_CD);
}

//---------------------------------------------------------------------------
//
//	CDシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetCD(BOOL ast)
{
	SetSignal(PIN_CD, ast);
}

//---------------------------------------------------------------------------
//
//	IOシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetIO()
{
	BOOL ast;
	ast = GetSignal(PIN_IO);

	if (actmode == INITIATOR) {
		// IO信号によってデータの入出力方向を変更
		if (ast) {
			SetControl(PIN_DTD, DTD_IN);
			SetMode(PIN_DT0, IN);
			SetMode(PIN_DT1, IN);
			SetMode(PIN_DT2, IN);
			SetMode(PIN_DT3, IN);
			SetMode(PIN_DT4, IN);
			SetMode(PIN_DT5, IN);
			SetMode(PIN_DT6, IN);
			SetMode(PIN_DT7, IN);
			SetMode(PIN_DP, IN);
		} else {
			SetControl(PIN_DTD, DTD_OUT);
			SetMode(PIN_DT0, OUT);
			SetMode(PIN_DT1, OUT);
			SetMode(PIN_DT2, OUT);
			SetMode(PIN_DT3, OUT);
			SetMode(PIN_DT4, OUT);
			SetMode(PIN_DT5, OUT);
			SetMode(PIN_DT6, OUT);
			SetMode(PIN_DT7, OUT);
			SetMode(PIN_DP, OUT);
		}
	}

	return ast;
}

//---------------------------------------------------------------------------
//
//	IOシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetIO(BOOL ast)
{
	SetSignal(PIN_IO, ast);

	if (actmode == TARGET) {
		// IO信号によってデータの入出力方向を変更
		if (ast) {
			SetControl(PIN_DTD, DTD_OUT);
			SetMode(PIN_DT0, OUT);
			SetMode(PIN_DT1, OUT);
			SetMode(PIN_DT2, OUT);
			SetMode(PIN_DT3, OUT);
			SetMode(PIN_DT4, OUT);
			SetMode(PIN_DT5, OUT);
			SetMode(PIN_DT6, OUT);
			SetMode(PIN_DT7, OUT);
			SetMode(PIN_DP, OUT);
		} else {
			SetControl(PIN_DTD, DTD_IN);
			SetMode(PIN_DT0, IN);
			SetMode(PIN_DT1, IN);
			SetMode(PIN_DT2, IN);
			SetMode(PIN_DT3, IN);
			SetMode(PIN_DT4, IN);
			SetMode(PIN_DT5, IN);
			SetMode(PIN_DT6, IN);
			SetMode(PIN_DT7, IN);
			SetMode(PIN_DP, IN);
		}
	}
}

//---------------------------------------------------------------------------
//
//	REQシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetREQ()
{
	return GetSignal(PIN_REQ);
}

//---------------------------------------------------------------------------
//
//	REQシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetREQ(BOOL ast)
{
	SetSignal(PIN_REQ, ast);
}

//---------------------------------------------------------------------------
//
//	データシグナル取得
//
//---------------------------------------------------------------------------
BYTE FASTCALL GPIOBUS::GetDAT()
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
//	データシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetDAT(BYTE dat)
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
//	データ受信ハンドシェイク
//
//---------------------------------------------------------------------------
int FASTCALL GPIOBUS::ReceiveHandShake(BYTE *buf, int count)
{
	int i;
	DWORD loop;
	DWORD phase;

	if (drvfd >= 0) {
		return read(drvfd, buf, count);
	} else {
		if (actmode == TARGET) {
			for (i = 0; i < count; i++) {
				// REQアサート
				SetSignal(PIN_REQ, ON);

				// ACKアサート待ち
				loop = 30000000;
				do {
					Aquire();
					loop--;
				} while (!(signals & (1 << PIN_ACK)) &&
					!(signals & (1 << PIN_RST)) && loop > 0);

				// データ取得
				*buf = GetDAT();

				// REQネゲート
				SetSignal(PIN_REQ, OFF);

				// ACKアサート待ちでタイムアウト
				if (loop == 0 || (signals & (1 << PIN_RST))) {
					break;
				}

				// ACKネゲート待ち
				loop = 30000000;
				do {
					Aquire();
					loop--;
				} while ((signals & (1 << PIN_ACK)) &&
					!(signals & (1 << PIN_RST)) && loop > 0);

				// ACKネゲート待ちでタイムアウト
				if (loop == 0 || (signals & (1 << PIN_RST))) {
					break;
				}

				// 次データへ
				buf++;
			}

			// 受信数を返却
			return i;
		} else {
			// フェーズ取得
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
				*buf = GetDAT();

				// ACKアサート
				SetSignal(PIN_ACK, ON);

				// REQネゲート待ち
				loop = 30000000;
				do {
					Aquire();
					loop--;
				} while ((signals & (1 << PIN_REQ)) &&
					((signals & GPIO_MCI) == phase) && loop > 0);

				// ACKネゲート
				SetSignal(PIN_ACK, OFF);

				// REQネゲート待ちでタイムアウト
				if (loop == 0 || (signals & GPIO_MCI) != phase) {
					break;
				}

				// 次データへ
				buf++;
			}

			// 受信数を返却
			return i;
		}
	}
}

//---------------------------------------------------------------------------
//
//	データ送信ハンドシェイク
//
//---------------------------------------------------------------------------
int FASTCALL GPIOBUS::SendHandShake(BYTE *buf, int count)
{
	int i;
	DWORD loop;
	DWORD phase;

	if (drvfd >= 0) {
		return write(drvfd, buf, count);
	} else {
		if (actmode == TARGET) {
			for (i = 0; i < count; i++) {
				// データ設定
				SetDAT(*buf);

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
				SetSignal(PIN_REQ, ON);

				// ACKアサート待ち
				loop = (DWORD)30000000;
				do {
					Aquire();
					loop--;
				} while (!(signals & (1 << PIN_ACK)) &&
					!(signals & (1 << PIN_RST)) && loop > 0);

				// REQネゲート
				SetSignal(PIN_REQ, OFF);

				// ACKアサート待ちでタイムアウト
				if (loop == 0 || (signals & (1 << PIN_RST))) {
					break;
				}

				// 次データへ
				buf++;
			}

			// ACKネゲート待ち
			loop = (DWORD)30000000;
			do {
				Aquire();
				loop--;
			} while ((signals & (1 << PIN_ACK)) &&
				!(signals & (1 << PIN_RST)) && loop > 0);

			// 送信数を返却
			return i;
		} else {
			// フェーズ取得
			phase = signals & GPIO_MCI;

			for (i = 0; i < count; i++) {
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
				SetDAT(*buf);

				// 信号線が安定するまでウェイト
				SleepNsec(150);

				// ACKアサート
				SetSignal(PIN_ACK, ON);

				// REQネゲート待ち
				loop = (DWORD)30000000;
				do {
					Aquire();
					loop--;
				} while ((signals & (1 << PIN_REQ)) &&
					((signals & GPIO_MCI) == phase) && loop > 0);

				// ACKネゲート
				SetSignal(PIN_ACK, OFF);

				// REQネゲート待ちでタイムアウト
				if (loop == 0 || (signals & GPIO_MCI) != phase) {
					break;
				}

				// 次データへ
				buf++;
			}

			// 送信数を返却
			return i;
		}
	}
}

//---------------------------------------------------------------------------
//
//	信号テーブル
//
//---------------------------------------------------------------------------
const int GPIOBUS::SignalTable[19] = {
	PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3,
	PIN_DT4, PIN_DT5, PIN_DT6, PIN_DT7,	PIN_DP,
	PIN_SEL,PIN_ATN, PIN_RST, PIN_ACK,
	PIN_BSY, PIN_MSG, PIN_CD, PIN_IO, PIN_REQ,
	-1
};
