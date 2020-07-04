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
//	[ GPIO-SCSIバス ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "gpiobus.h"

#ifndef BAREMETAL
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
	if (address == 0) {
		address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
	}
	address = (address == (DWORD)~0) ? 0x20000000 : address;
#if 0
	printf("Peripheral address : 0x%lx\n", address);
#endif
	return address;
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
#endif	// __NetBSD__
#endif	// BAREMETAL

#ifdef BAREMETAL
// IOベースアドレス
extern uint32_t RPi_IO_Base_Addr;

// コア周波数
extern uint32_t RPi_Core_Freq;

#ifdef USE_SEL_EVENT_ENABLE 
//---------------------------------------------------------------------------
//
//	割り込み制御関数
//
//---------------------------------------------------------------------------
extern "C" {
extern uintptr_t setIrqFuncAddress (void(*ARMaddress)(void));
extern void EnableInterrupts (void);
extern void DisableInterrupts (void);
extern void WaitForInterrupts (void);
}

//---------------------------------------------------------------------------
//
//	割り込みハンドラ
//
//---------------------------------------------------------------------------
static GPIOBUS *self;
extern "C"
void IrqHandler()
{
	// 割り込みクリア
	self->ClearSelectEvent();
}
#endif	// USE_SEL_EVENT_ENABLE
#endif	// BAREMETAL

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
GPIOBUS::GPIOBUS()
{
#if defined(USE_SEL_EVENT_ENABLE) && defined(BAREMETAL)
	self = this;
#endif	// USE_SEL_EVENT_ENABLE && BAREMETAL
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
//	初期化
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::Init(mode_e mode)
{
	void *map;
	int i;
	int j;
	int pullmode;
#ifndef BAREMETAL
	int fd;
#ifdef USE_SEL_EVENT_ENABLE
	struct epoll_event ev;
#endif	// USE_SEL_EVENT_ENABLE
#endif	// BAREMETAL

	// 動作モードの保存
	actmode = mode;

#ifdef BAREMETAL
	// ベースアドレスの取得
	baseaddr = RPi_IO_Base_Addr;
	map = (void*)baseaddr;
#else
	// ベースアドレスの取得
	baseaddr = (DWORD)bcm_host_get_peripheral_address();

	// /dev/memオープン
	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd == -1) {
		return FALSE;
	}

	// ペリフェラルリージョンのメモリをマップ
	map = mmap(NULL, 0x1000100,
		PROT_READ | PROT_WRITE, MAP_SHARED, fd, baseaddr);
	if (map == MAP_FAILED) {
		close(fd);
		return FALSE;
	}
#endif
	// ベースアドレスからラズパイのタイプを決定
	if (baseaddr == 0xfe000000) {
		rpitype = 4;
	} else if (baseaddr == 0x3f000000) {
		rpitype = 2;
	} else {
		rpitype = 1;
	}

	// GPIO
	gpio = (DWORD *)map;
	gpio += GPIO_OFFSET / sizeof(DWORD);
	level = &gpio[GPIO_LEV_0];

	// PADS
	pads = (DWORD *)map;
	pads += PADS_OFFSET / sizeof(DWORD);

	// システムタイマ
	SysTimer::Init(
		(DWORD *)map + SYST_OFFSET / sizeof(DWORD),
		(DWORD *)map + ARMT_OFFSET / sizeof(DWORD));

	// 割り込みコントローラ
	irpctl = (DWORD *)map;
	irpctl += IRPT_OFFSET / sizeof(DWORD);

#ifndef BAREMETAL
	// Quad-A7 control
	qa7regs = (DWORD *)map;
	qa7regs += QA7_OFFSET / sizeof(DWORD);
#endif	// BAREMETAL

#ifdef BAREMETAL
	// GICのメモリをマップ
	if (rpitype == 4) {
		map = (void*)ARM_GICD_BASE;
		gicd = (DWORD *)map;
		map = (void*)ARM_GICC_BASE;
		gicc = (DWORD *)map;
	} else {
		gicd = NULL;
		gicc = NULL;
	}
#else
	// GICのメモリをマップ
	if (rpitype == 4) {
		map = mmap(NULL, 8192,
			PROT_READ | PROT_WRITE, MAP_SHARED, fd, ARM_GICD_BASE);
		if (map == MAP_FAILED) {
			close(fd);
			return FALSE;
		}
		gicd = (DWORD *)map;
		gicc = (DWORD *)map;
		gicc += (ARM_GICC_BASE - ARM_GICD_BASE) / sizeof(DWORD);
	} else {
		gicd = NULL;
		gicc = NULL;
	}
	close(fd);
#endif	// BAREMETAL

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

	// ENABLE信号を設定
	PinSetSignal(PIN_ENB, ENB_OFF);
	PinConfig(PIN_ENB, GPIO_OUTPUT);

	// GPFSELバックアップ
	gpfsel[0] = gpio[GPIO_FSEL_0];
	gpfsel[1] = gpio[GPIO_FSEL_1];
	gpfsel[2] = gpio[GPIO_FSEL_2];
	gpfsel[3] = gpio[GPIO_FSEL_3];

	// SEL信号割り込み初期化
#ifdef USE_SEL_EVENT_ENABLE
#ifndef BAREMETAL
	// GPIOチップオープン
	fd = open("/dev/gpiochip0", 0);
	if (fd == -1) {
		return FALSE;
	}

	// イベント要求設定
	strcpy(selevreq.consumer_label, "RaSCSI");
	selevreq.lineoffset = PIN_SEL;
	selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
#if SIGNAL_CONTROL_MODE < 2
	selevreq.eventflags = GPIOEVENT_REQUEST_FALLING_EDGE;
#else
	selevreq.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
#endif	// SIGNAL_CONTROL_MODE

	// イベント要求取得
	if (ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1) {
		close(fd);
		return FALSE;
	}

	// GPIOチップクローズ
	close(fd);

	// epoll初期化
	epfd = epoll_create(1);
	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN | EPOLLPRI;
	ev.data.fd = selevreq.fd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, selevreq.fd, &ev);
#else
	// エッジ検出設定
#if SIGNAL_CONTROL_MODE == 2
	gpio[GPIO_AREN_0] = 1 << PIN_SEL;
#else
	gpio[GPIO_AFEN_0] = 1 << PIN_SEL;
#endif	// SIGNAL_CONTROL_MODE

	// イベントクリア
	gpio[GPIO_EDS_0] = 1 << PIN_SEL;

	// 割り込みハンドラ登録
	setIrqFuncAddress(IrqHandler);

	// GPIO割り込み設定
	if (rpitype == 4) {
		// GIC無効
		gicd[GICD_CTLR] = 0;

		// 全ての割り込みをコア0にルーティング
		for (i = 0; i < 8; i++) {
			gicd[GICD_ICENABLER0 + i] = 0xffffffff;
			gicd[GICD_ICPENDR0 + i] = 0xffffffff;
			gicd[GICD_ICACTIVER0 + i] = 0xffffffff;
		}
		for (i = 0; i < 64; i++) {
			gicd[GICD_IPRIORITYR0 + i] = 0xa0a0a0a0;
			gicd[GICD_ITARGETSR0 + i] = 0x01010101;
		}

		// 全ての割り込みをレベルトリガに設定
		for (i = 0; i < 16; i++) {
			gicd[GICD_ICFGR0 + i] = 0;
		}

		// GIC無効
		gicd[GICD_CTLR] = 1;

		// コア0のCPUインターフェスを有効にする
		gicc[GICC_PMR] = 0xf0;
		gicc[GICC_CTLR] = 1;

		// 割り込み有効
		gicd[GICD_ISENABLER0 + (GIC_GPIO_IRQ / 32)] =
			1 << (GIC_GPIO_IRQ % 32);
	} else {
		// 割り込み有効
		irpctl[IRPT_ENB_IRQ_2] = (1 << (GPIO_IRQ % 32));
	}
#endif	// BAREMETAL
#endif	// USE_SEL_EVENT_ENABLE

	// ワークテーブル作成
	MakeTable();

	// 最後にENABLEをオン
	SetControl(PIN_ENB, ENB_ON);

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
	int pin;

	// SEL信号割り込み解放
#ifdef USE_SEL_EVENT_ENABLE
#ifndef BAREMETAL
	close(selevreq.fd);
#endif	// BAREMETAL
#endif	// USE_SEL_EVENT_ENABLE

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
		pin = SignalTable[i];
		PinSetSignal(pin, FALSE);
		PinConfig(pin, GPIO_INPUT);
		PullConfig(pin, GPIO_PULLNONE);
	}

	// Drive Strengthを8mAに設定
	DrvConfig(3);
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
#endif	// SIGNAL_CONTROL_MODE
	
	return signals;
}

//---------------------------------------------------------------------------
//
//	ENBシグナル設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::SetENB(BOOL ast)
{
	PinSetSignal(PIN_ENB, ast ? ENB_ON : ENB_OFF);
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
	// BSY信号を設定
	SetSignal(PIN_BSY, ast);

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
			// アクティブ信号をオフ
			SetControl(PIN_ACT, ACT_OFF);

			// ターゲット信号を入力に設定
			SetControl(PIN_TAD, TAD_IN);

			SetMode(PIN_BSY, IN);
			SetMode(PIN_MSG, IN);
			SetMode(PIN_CD, IN);
			SetMode(PIN_REQ, IN);
			SetMode(PIN_IO, IN);
		}
	}
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
			SetDAT(0);
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
	DWORD fsel;

	fsel = gpfsel[0];
	fsel &= tblDatMsk[0][dat];
	fsel |= tblDatSet[0][dat];
	if (fsel != gpfsel[0]) {
		gpfsel[0] = fsel;
		gpio[GPIO_FSEL_0] = fsel;
	}

	fsel = gpfsel[1];
	fsel &= tblDatMsk[1][dat];
	fsel |= tblDatSet[1][dat];
	if (fsel != gpfsel[1]) {
		gpfsel[1] = fsel;
		gpio[GPIO_FSEL_1] = fsel;
	}

	fsel = gpfsel[2];
	fsel &= tblDatMsk[2][dat];
	fsel |= tblDatSet[2][dat];
	if (fsel != gpfsel[2]) {
		gpfsel[2] = fsel;
		gpio[GPIO_FSEL_2] = fsel;
	}
#else
	gpio[GPIO_CLR_0] = tblDatMsk[dat];
	gpio[GPIO_SET_0] = tblDatSet[dat];
#endif	// SIGNAL_CONTROL_MODE
}

//---------------------------------------------------------------------------
//
//	データパリティシグナル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::GetDP()
{
	return GetSignal(PIN_DP);
}

//---------------------------------------------------------------------------
//
//	コマンド受信ハンドシェイク
//
//---------------------------------------------------------------------------
int FASTCALL GPIOBUS::CommandHandShake(BYTE *buf)
{
	int i;
	BOOL ret;
	int count;

	// ターゲットモードのみ
	if (actmode != TARGET) {
		return 0;
	}

	// IRQ無効
	DisableIRQ();

	// 最初のコマンドバイトを取得
	i = 0;

	// REQアサート
	SetSignal(PIN_REQ, ON);

	// ACKアサート待ち
	ret = WaitSignal(PIN_ACK, TRUE);

	// 信号線が安定するまでウェイト
	SysTimer::SleepNsec(GPIO_DATA_SETTLING);

	// データ取得
	*buf = GetDAT();

	// REQネゲート
	SetSignal(PIN_REQ, OFF);

	// ACKアサート待ちでタイムアウト
	if (!ret) {
		goto irq_enable_exit;
	}

	// ACKネゲート待ち
	ret = WaitSignal(PIN_ACK, FALSE);

	// ACKネゲート待ちでタイムアウト
	if (!ret) {
		goto irq_enable_exit;
	}

	// コマンドが6バイトか10バイトか見分ける
	if (*buf >= 0x20 && *buf <= 0x7D) {
		count = 10;
	} else {
		count = 6;
	}

	// 次データへ
	buf++;

	for (i = 1; i < count; i++) {
		// REQアサート
		SetSignal(PIN_REQ, ON);

		// ACKアサート待ち
		ret = WaitSignal(PIN_ACK, TRUE);

		// 信号線が安定するまでウェイト
		SysTimer::SleepNsec(GPIO_DATA_SETTLING);

		// データ取得
		*buf = GetDAT();

		// REQネゲート
		SetSignal(PIN_REQ, OFF);

		// ACKアサート待ちでタイムアウト
		if (!ret) {
			break;
		}

		// ACKネゲート待ち
		ret = WaitSignal(PIN_ACK, FALSE);

		// ACKネゲート待ちでタイムアウト
		if (!ret) {
			break;
		}

		// 次データへ
		buf++;
	}

irq_enable_exit:
	// IRQ有効
	EnableIRQ();

	// 受信数を返却
	return i;
}

//---------------------------------------------------------------------------
//
//	データ受信ハンドシェイク
//
//---------------------------------------------------------------------------
int FASTCALL GPIOBUS::ReceiveHandShake(BYTE *buf, int count)
{
	int i;
	BOOL ret;
	DWORD phase;

	// IRQ無効
	DisableIRQ();

	if (actmode == TARGET) {
		for (i = 0; i < count; i++) {
			// REQアサート
			SetSignal(PIN_REQ, ON);

			// ACKアサート待ち
			ret = WaitSignal(PIN_ACK, TRUE);

			// 信号線が安定するまでウェイト
			SysTimer::SleepNsec(GPIO_DATA_SETTLING);

			// データ取得
			*buf = GetDAT();

			// REQネゲート
			SetSignal(PIN_REQ, OFF);

			// ACKアサート待ちでタイムアウト
			if (!ret) {
				break;
			}

			// ACKネゲート待ち
			ret = WaitSignal(PIN_ACK, FALSE);

			// ACKネゲート待ちでタイムアウト
			if (!ret) {
				break;
			}

			// 次データへ
			buf++;
		}
	} else {
		// フェーズ取得
		phase = Aquire() & GPIO_MCI;

		for (i = 0; i < count; i++) {
			// REQアサート待ち
			ret = WaitSignal(PIN_REQ, TRUE);

			// REQアサート待ちでタイムアウト
			if (!ret) {
				break;
			}

			// フェーズエラー
			if ((signals & GPIO_MCI) != phase) {
				break;
			}

			// 信号線が安定するまでウェイト
			SysTimer::SleepNsec(GPIO_DATA_SETTLING);

			// データ取得
			*buf = GetDAT();

			// ACKアサート
			SetSignal(PIN_ACK, ON);

			// REQネゲート待ち
			ret = WaitSignal(PIN_REQ, FALSE);

			// ACKネゲート
			SetSignal(PIN_ACK, OFF);

			// REQネゲート待ちでタイムアウト
			if (!ret) {
				break;
			}

			// フェーズエラー
			if ((signals & GPIO_MCI) != phase) {
				break;
			}

			// 次データへ
			buf++;
		}
	}

	// IRQ有効
	EnableIRQ();

	// 受信数を返却
	return i;
}

//---------------------------------------------------------------------------
//
//	データ送信ハンドシェイク
//
//---------------------------------------------------------------------------
int FASTCALL GPIOBUS::SendHandShake(BYTE *buf, int count)
{
	int i;
	BOOL ret;
	DWORD phase;

	// IRQ無効
	DisableIRQ();

	if (actmode == TARGET) {
		for (i = 0; i < count; i++) {
			// データ設定
			SetDAT(*buf);

			// ACKネゲート待ち
			ret = WaitSignal(PIN_ACK, FALSE);

			// ACKネゲート待ちでタイムアウト
			if (!ret) {
				break;
			}

			// ACKネゲート待ちで既にウェイトが入っている

			// REQアサート
			SetSignal(PIN_REQ, ON);

			// ACKアサート待ち
			ret = WaitSignal(PIN_ACK, TRUE);

			// REQネゲート
			SetSignal(PIN_REQ, OFF);

			// ACKアサート待ちでタイムアウト
			if (!ret) {
				break;
			}

			// 次データへ
			buf++;
		}

		// ACKネゲート待ち
		WaitSignal(PIN_ACK, FALSE);
	} else {
		// フェーズ取得
		phase = Aquire() & GPIO_MCI;

		for (i = 0; i < count; i++) {
			// データ設定
			SetDAT(*buf);

			// REQアサート待ち
			ret = WaitSignal(PIN_REQ, TRUE);

			// REQアサート待ちでタイムアウト
			if (!ret) {
				break;
			}

			// フェーズエラー
			if ((signals & GPIO_MCI) != phase) {
				break;
			}

			// REQアサート待ちで既にウェイトが入っている

			// ACKアサート
			SetSignal(PIN_ACK, ON);

			// REQネゲート待ち
			ret = WaitSignal(PIN_REQ, FALSE);

			// ACKネゲート
			SetSignal(PIN_ACK, OFF);

			// REQネゲート待ちでタイムアウト
			if (!ret) {
				break;
			}

			// フェーズエラー
			if ((signals & GPIO_MCI) != phase) {
				break;
			}

			// 次データへ
			buf++;
		}
	}

	// IRQ有効
	EnableIRQ();

	// 送信数を返却
	return i;
}

#ifdef USE_SEL_EVENT_ENABLE
//---------------------------------------------------------------------------
//
//	SEL信号イベントポーリング
//
//---------------------------------------------------------------------------
int FASTCALL GPIOBUS::PollSelectEvent()
{
	// errnoクリア
	errno = 0;

#ifdef BAREMETAL
	// 割り込み有効
	EnableInterrupts();

	// 割り込み待ち
	WaitForInterrupts();

	// 割り込み無効
	DisableInterrupts();
#else
	struct epoll_event epev;
	struct gpioevent_data gpev;

	if (epoll_wait(epfd, &epev, 1, -1) <= 0) {
		return -1;
	}

	read(selevreq.fd, &gpev, sizeof(gpev));
#endif	// BAREMETAL

	return 0;
}

//---------------------------------------------------------------------------
//
//	SEL信号イベント解除
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::ClearSelectEvent()
{
#ifdef BAREMETAL
	DWORD irq;

	// イベントクリア
	gpio[GPIO_EDS_0] = 1 << PIN_SEL;

	// GICへの応答
	if (rpitype == 4) {
		// IRQ番号
		irq = gicc[GICC_IAR] & 0x3FF;

		// 割り込み応答
		gicc[GICC_EOIR] = irq;
	}
#endif	// BAREMETAL
}
#endif	// USE_SEL_EVENT_ENABLE

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
#endif	// SIGNAL_CONTROL_MODE

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
	if (ast) {
		data |= (1 << shift);
	} else {
		data &= ~(0x7 << shift);
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
#endif	// SIGNAL_CONTROL_MODE
}

//---------------------------------------------------------------------------
//
//	信号変化待ち
//
//---------------------------------------------------------------------------
BOOL FASTCALL GPIOBUS::WaitSignal(int pin, BOOL ast)
{
	DWORD now;
	DWORD timeout;

	// 現在
	now = SysTimer::GetTimerLow();

	// タイムアウト時間(3000ms)
	timeout = 3000 * 1000;

	// 変化したら即終了
	do {
		// リセットを受信したら即終了
		Aquire();
		if (GetRST()) {
			return FALSE;
		}

		// エッジを検出したら
		if (((signals >> pin) ^ ~ast) & 1) {
			return TRUE;
		}
	} while ((SysTimer::GetTimerLow() - now) < timeout);

	// タイムアウト
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	IRQ禁止
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::DisableIRQ()
{
#ifndef BAREMETAL
	if (rpitype == 4) {
		// RPI4はGICCで割り込み禁止に設定
		giccpmr = gicc[GICC_PMR];
		gicc[GICC_PMR] = 0;
	} else if (rpitype == 2) {
		// RPI2,3はコアタイマーIRQを無効にする
		tintcore = sched_getcpu() + QA7_CORE0_TINTC;
		tintctl = qa7regs[tintcore];
		qa7regs[tintcore] = 0;
	} else {
		// 割り込みコントローラでシステムタイマー割り込みを止める
		irptenb = irpctl[IRPT_ENB_IRQ_1];
		irpctl[IRPT_DIS_IRQ_1] = irptenb & 0xf;
	}
#endif	// BAREMETAL
}

//---------------------------------------------------------------------------
//
//	IRQ有効
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::EnableIRQ()
{
#ifndef BAREMETAL
	if (rpitype == 4) {
		// RPI4はGICCを割り込み許可に設定
		gicc[GICC_PMR] = giccpmr;
	} else if (rpitype == 2) {
		// RPI2,3はコアタイマーIRQを有効に戻す
		qa7regs[tintcore] = tintctl;
	} else {
		// 割り込みコントローラでシステムタイマー割り込みを再開
		irpctl[IRPT_ENB_IRQ_1] = irptenb & 0xf;
	}
#endif	// BAREMETAL
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
	int shift;
	DWORD bits;
	DWORD pull;

	// 未使用なら無効
	if (pin < 0) {
		return;
	}

	if (rpitype == 4) {
		switch (mode) {
			case GPIO_PULLNONE:
				pull = 0;
				break;
			case GPIO_PULLUP:
				pull = 1;
				break;
			case GPIO_PULLDOWN:
				pull = 2;
				break;
			default:
				return;
		}

		pin &= 0x1f;
		shift = (pin & 0xf) << 1;
		bits = gpio[GPIO_PUPPDN0 + (pin >> 4)];
		bits &= ~(3 << shift);
		bits |= (pull << shift);
		gpio[GPIO_PUPPDN0 + (pin >> 4)] = bits;
	} else {
		pin &= 0x1f;
		gpio[GPIO_PUD] = mode & 0x3;
		SysTimer::SleepUsec(2);
		gpio[GPIO_CLK_0] = 0x1 << pin;
		SysTimer::SleepUsec(2);
		gpio[GPIO_PUD] = 0;
		gpio[GPIO_CLK_0] = 0;
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
//	Drive Strength設定
//
//---------------------------------------------------------------------------
void FASTCALL GPIOBUS::DrvConfig(DWORD drive)
{
	DWORD data;

	data = pads[PAD_0_27];
	pads[PAD_0_27] = (0xFFFFFFF8 & data) | drive | 0x5a000000;
}

//---------------------------------------------------------------------------
//
//	システムタイマアドレス
//
//---------------------------------------------------------------------------
volatile DWORD* SysTimer::systaddr;

//---------------------------------------------------------------------------
//
//	ARMタイマアドレス
//
//---------------------------------------------------------------------------
volatile DWORD* SysTimer::armtaddr;

//---------------------------------------------------------------------------
//
//	コア周波数
//
//---------------------------------------------------------------------------
volatile DWORD SysTimer::corefreq;

//---------------------------------------------------------------------------
//
//	システムタイマ初期化
//
//---------------------------------------------------------------------------
void FASTCALL SysTimer::Init(DWORD *syst, DWORD *armt)
{
#ifndef BAREMETAL
	// RPI Mailbox property interface
	// Get max clock rate
	//  Tag: 0x00030004
	//
	//  Request: Length: 4
	//   Value: u32: clock id
	//  Response: Length: 8
	//   Value: u32: clock id, u32: rate (in Hz)
	//
	// Clock id
	//  0x000000004: CORE
	DWORD maxclock[32] = { 32, 0, 0x00030004, 8, 0, 4, 0, 0 };
	int fd;
#endif	// BAREMETAL

	// ベースアドレス保存
	systaddr = syst;
	armtaddr = armt;

	// ARMタイマをフリーランモードに変更
	armtaddr[ARMT_CTRL] = 0x00000282;

	// コア周波数取得
#ifdef BAREMETAL
	corefreq = RPi_Core_Freq / 1000000;
#else
	corefreq = 0;
	fd = open("/dev/vcio", O_RDONLY);
	if (fd >= 0) {
		ioctl(fd, _IOWR(100, 0, char *), maxclock);
		corefreq = maxclock[6] / 1000000;
	}
	close(fd);
#endif	// BAREMETAL
}

//---------------------------------------------------------------------------
//
//	システムタイマ(LO)取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL SysTimer::GetTimerLow() {
	return systaddr[SYST_CLO];
}

//---------------------------------------------------------------------------
//
//	システムタイマ(HI)取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL SysTimer::GetTimerHigh() {
	return systaddr[SYST_CHI];
}

//---------------------------------------------------------------------------
//
//	ナノ秒単位のスリープ
//
//---------------------------------------------------------------------------
void FASTCALL SysTimer::SleepNsec(DWORD nsec)
{
	DWORD diff;
	DWORD start;

	// ウェイトしない
	if (nsec == 0) {
		return;
	}

	// タイマー差を算出
	diff = corefreq * nsec / 1000;

	// 微小なら復帰
	if (diff == 0) {
		return;
	}

	// 開始
	start = armtaddr[ARMT_FREERUN];

	// カウントオーバーまでループ
	while ((armtaddr[ARMT_FREERUN] - start) < diff);
}

//---------------------------------------------------------------------------
//
//	μ秒単位のスリープ
//
//---------------------------------------------------------------------------
void FASTCALL SysTimer::SleepUsec(DWORD usec)
{
	DWORD now;

	// ウェイトしない
	if (usec == 0) {
		return;
	}

	now = GetTimerLow();
	while ((GetTimerLow() - now) < usec);
}
