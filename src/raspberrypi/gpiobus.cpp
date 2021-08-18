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
//	[ GPIO-SCSI bus ]
//
//---------------------------------------------------------------------------

#include <sys/mman.h>

#include "os.h"
#include "xm6.h"
#include "gpiobus.h"
#include "log.h"

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
// Assume the Raspberry Pi series and estimate the address from CPU
DWORD bcm_host_get_peripheral_address(void)
{
	char buf[1024];
	size_t len = sizeof(buf);
	DWORD address;

	if (sysctlbyname("hw.model", buf, &len, NULL, 0) ||
	    strstr(buf, "ARM1176JZ-S") != buf) {
		// Failed to get CPU model || Not BCM2835
        // use the address of BCM283[67]
		address = 0x3f000000;
	} else {
		// Use BCM2835 address
		address = 0x20000000;
	}
	printf("Peripheral address : 0x%lx\n", address);
	return address;
}
#endif	// __NetBSD__

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
GPIOBUS::GPIOBUS()
{
	actmode = TARGET;
	baseaddr = 0;
	gicc = 0;
	gicd = 0;
	gpio = 0;
	level = 0;
	pads = 0;
	irpctl = 0;
	qa7regs = 0;
	signals = 0;
	rpitype = 0;
}

//---------------------------------------------------------------------------
//
//	Destructor
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
BOOL GPIOBUS::Init(mode_e mode)
{
#if defined(__x86_64__) || defined(__X86__)
	// When we're running on x86, there is no hardware to talk to, so just return.
	return true;
#else
	void *map;
	int i;
	int j;
	int pullmode;
	int fd;
#ifdef USE_SEL_EVENT_ENABLE
	struct epoll_event ev;
#endif	// USE_SEL_EVENT_ENABLE

	// Save operation mode
	actmode = mode;

	// Get the base address
	baseaddr = (DWORD)bcm_host_get_peripheral_address();

	// Open /dev/mem
	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd == -1) {
        LOGERROR("Error: Unable to open /dev/mem. Are you running as root?");
		return FALSE;
	}

	// Map peripheral region memory
	map = mmap(NULL, 0x1000100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, baseaddr);
	if (map == MAP_FAILED) {
        LOGERROR("Error: Unable to map memory");
		close(fd);
		return FALSE;
	}

	// Determine the type of raspberry pi from the base address
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

	// System timer
	SysTimer::Init(
		(DWORD *)map + SYST_OFFSET / sizeof(DWORD),
		(DWORD *)map + ARMT_OFFSET / sizeof(DWORD));

	// Interrupt controller
	irpctl = (DWORD *)map;
	irpctl += IRPT_OFFSET / sizeof(DWORD);

	// Quad-A7 control
	qa7regs = (DWORD *)map;
	qa7regs += QA7_OFFSET / sizeof(DWORD);

	// Map GIC memory
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

	// Set Drive Strength to 16mA
	DrvConfig(7);

	// Set pull up/pull down
#if SIGNAL_CONTROL_MODE == 0
	pullmode = GPIO_PULLNONE;
#elif SIGNAL_CONTROL_MODE == 1
	pullmode = GPIO_PULLUP;
#else
	pullmode = GPIO_PULLDOWN;
#endif

	// Initialize all signals
	for (i = 0; SignalTable[i] >= 0; i++) {
		j = SignalTable[i];
		PinSetSignal(j, FALSE);
		PinConfig(j, GPIO_INPUT);
		PullConfig(j, pullmode);
	}

	// Set control signals
	PinSetSignal(PIN_ACT, FALSE);
	PinSetSignal(PIN_TAD, FALSE);
	PinSetSignal(PIN_IND, FALSE);
	PinSetSignal(PIN_DTD, FALSE);
	PinConfig(PIN_ACT, GPIO_OUTPUT);
	PinConfig(PIN_TAD, GPIO_OUTPUT);
	PinConfig(PIN_IND, GPIO_OUTPUT);
	PinConfig(PIN_DTD, GPIO_OUTPUT);

	// Set the ENABLE signal
	// This is used to show that the application is running
	PinSetSignal(PIN_ENB, ENB_OFF);
	PinConfig(PIN_ENB, GPIO_OUTPUT);

	// GPFSEL backup
	gpfsel[0] = gpio[GPIO_FSEL_0];
	gpfsel[1] = gpio[GPIO_FSEL_1];
	gpfsel[2] = gpio[GPIO_FSEL_2];
	gpfsel[3] = gpio[GPIO_FSEL_3];

	// Initialize SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
	// GPIO chip open
	fd = open("/dev/gpiochip0", 0);
	if (fd == -1) {
		LOGERROR("Unable to open /dev/gpiochip0. Is RaSCSI already running?")
		return FALSE;
	}

	// Event request setting
	strcpy(selevreq.consumer_label, "RaSCSI");
	selevreq.lineoffset = PIN_SEL;
	selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
#if SIGNAL_CONTROL_MODE < 2
	selevreq.eventflags = GPIOEVENT_REQUEST_FALLING_EDGE;
#else
	selevreq.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
#endif	// SIGNAL_CONTROL_MODE

	//Get event request
	if (ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1) {
		LOGERROR("Unable to register event request. Is RaSCSI already running?")
		close(fd);
		return FALSE;
	}

	// Close GPIO chip file handle
	close(fd);

	// epoll initialization
	epfd = epoll_create(1);
	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN | EPOLLPRI;
	ev.data.fd = selevreq.fd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, selevreq.fd, &ev);
#else
	// Edge detection setting
#if SIGNAL_CONTROL_MODE == 2
	gpio[GPIO_AREN_0] = 1 << PIN_SEL;
#else
	gpio[GPIO_AFEN_0] = 1 << PIN_SEL;
#endif	// SIGNAL_CONTROL_MODE

	// Clear event
	gpio[GPIO_EDS_0] = 1 << PIN_SEL;

	// Register interrupt handler
	setIrqFuncAddress(IrqHandler);

	// GPIO interrupt setting
	if (rpitype == 4) {
		// GIC Invalid
		gicd[GICD_CTLR] = 0;

		// Route all interupts to core 0
		for (i = 0; i < 8; i++) {
			gicd[GICD_ICENABLER0 + i] = 0xffffffff;
			gicd[GICD_ICPENDR0 + i] = 0xffffffff;
			gicd[GICD_ICACTIVER0 + i] = 0xffffffff;
		}
		for (i = 0; i < 64; i++) {
			gicd[GICD_IPRIORITYR0 + i] = 0xa0a0a0a0;
			gicd[GICD_ITARGETSR0 + i] = 0x01010101;
		}

		// Set all interrupts as level triggers
		for (i = 0; i < 16; i++) {
			gicd[GICD_ICFGR0 + i] = 0;
		}

		// GIC Invalid
		gicd[GICD_CTLR] = 1;

		// Enable CPU interface for core 0
		gicc[GICC_PMR] = 0xf0;
		gicc[GICC_CTLR] = 1;

		// Enable interrupts
		gicd[GICD_ISENABLER0 + (GIC_GPIO_IRQ / 32)] =
			1 << (GIC_GPIO_IRQ % 32);
	} else {
		// Enable interrupts
		irpctl[IRPT_ENB_IRQ_2] = (1 << (GPIO_IRQ % 32));
	}
#endif	// USE_SEL_EVENT_ENABLE

	// Create work table
	MakeTable();

	// Finally, enable ENABLE
	// Show the user that this app is running
	SetControl(PIN_ENB, ENB_ON);

	return TRUE;
#endif // ifdef __x86_64__ || __X86__

}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void GPIOBUS::Cleanup()
{
#if defined(__x86_64__) || defined(__X86__)
	return;
#else
	int i;
	int pin;

	// Release SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
	close(selevreq.fd);
#endif	// USE_SEL_EVENT_ENABLE

	// Set control signals
	PinSetSignal(PIN_ENB, FALSE);
	PinSetSignal(PIN_ACT, FALSE);
	PinSetSignal(PIN_TAD, FALSE);
	PinSetSignal(PIN_IND, FALSE);
	PinSetSignal(PIN_DTD, FALSE);
	PinConfig(PIN_ACT, GPIO_INPUT);
	PinConfig(PIN_TAD, GPIO_INPUT);
	PinConfig(PIN_IND, GPIO_INPUT);
	PinConfig(PIN_DTD, GPIO_INPUT);

	// Initialize all signals
	for (i = 0; SignalTable[i] >= 0; i++) {
		pin = SignalTable[i];
		PinSetSignal(pin, FALSE);
		PinConfig(pin, GPIO_INPUT);
		PullConfig(pin, GPIO_PULLNONE);
	}

	// Set drive strength back to 8mA
	DrvConfig(3);
#endif // ifdef __x86_64__ || __X86__
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void GPIOBUS::Reset()
{
#if defined(__x86_64__) || defined(__X86__)
	return;
#else
	int i;
	int j;

	// Turn off active signal
	SetControl(PIN_ACT, ACT_OFF);

	// Set all signals to off
	for (i = 0;; i++) {
		j = SignalTable[i];
		if (j < 0) {
			break;
		}

		SetSignal(j, OFF);
	}

	if (actmode == TARGET) {
		// Target mode

		// Set target signal to input
		SetControl(PIN_TAD, TAD_IN);
		SetMode(PIN_BSY, IN);
		SetMode(PIN_MSG, IN);
		SetMode(PIN_CD, IN);
		SetMode(PIN_REQ, IN);
		SetMode(PIN_IO, IN);

		// Set the initiator signal to input
		SetControl(PIN_IND, IND_IN);
		SetMode(PIN_SEL, IN);
		SetMode(PIN_ATN, IN);
		SetMode(PIN_ACK, IN);
		SetMode(PIN_RST, IN);

		// Set data bus signals to input
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
		// Initiator mode

		// Set target signal to input
		SetControl(PIN_TAD, TAD_IN);
		SetMode(PIN_BSY, IN);
		SetMode(PIN_MSG, IN);
		SetMode(PIN_CD, IN);
		SetMode(PIN_REQ, IN);
		SetMode(PIN_IO, IN);

		// Set the initiator signal to output
		SetControl(PIN_IND, IND_OUT);
		SetMode(PIN_SEL, OUT);
		SetMode(PIN_ATN, OUT);
		SetMode(PIN_ACK, OUT);
		SetMode(PIN_RST, OUT);

		// Set the data bus signals to outpu
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

	// Initialize all signals
	signals = 0;
#endif // ifdef __x86_64__ || __X86__
}

//---------------------------------------------------------------------------
//
//	ENB signal setting
//
//---------------------------------------------------------------------------
void GPIOBUS::SetENB(BOOL ast)
{
	PinSetSignal(PIN_ENB, ast ? ENB_ON : ENB_OFF);
}

//---------------------------------------------------------------------------
//
//	Get BSY signal
//
//---------------------------------------------------------------------------
bool GPIOBUS::GetBSY()
{
	return GetSignal(PIN_BSY);
}

//---------------------------------------------------------------------------
//
//	Set BSY signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetBSY(bool ast)
{
	// Set BSY signal
	SetSignal(PIN_BSY, ast);

	if (actmode == TARGET) {
		if (ast) {
			// Turn on ACTIVE signal
			SetControl(PIN_ACT, ACT_ON);

			// Set Target signal to output
			SetControl(PIN_TAD, TAD_OUT);

			SetMode(PIN_BSY, OUT);
			SetMode(PIN_MSG, OUT);
			SetMode(PIN_CD, OUT);
			SetMode(PIN_REQ, OUT);
			SetMode(PIN_IO, OUT);
		} else {
			// Turn off the ACTIVE signal
			SetControl(PIN_ACT, ACT_OFF);

			// Set the target signal to input
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
//	Get SEL signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetSEL()
{
	return GetSignal(PIN_SEL);
}

//---------------------------------------------------------------------------
//
//	Set SEL signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetSEL(BOOL ast)
{
	if (actmode == INITIATOR && ast) {
		// Turn on ACTIVE signal
		SetControl(PIN_ACT, ACT_ON);
	}

	// Set SEL signal
	SetSignal(PIN_SEL, ast);
}

//---------------------------------------------------------------------------
//
//	Get ATN signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetATN()
{
	return GetSignal(PIN_ATN);
}

//---------------------------------------------------------------------------
//
//	Get ATN signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetATN(BOOL ast)
{
	SetSignal(PIN_ATN, ast);
}

//---------------------------------------------------------------------------
//
//	Get ACK signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetACK()
{
	return GetSignal(PIN_ACK);
}

//---------------------------------------------------------------------------
//
//	Set ACK signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetACK(BOOL ast)
{
	SetSignal(PIN_ACK, ast);
}

//---------------------------------------------------------------------------
//
//	Get ACK signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetACT()
{
	return GetSignal(PIN_ACT);
}

//---------------------------------------------------------------------------
//
//	Set ACK signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetACT(BOOL ast)
{
	SetSignal(PIN_ACT, ast);
}

//---------------------------------------------------------------------------
//
//	Get RST signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetRST()
{
	return GetSignal(PIN_RST);
}

//---------------------------------------------------------------------------
//
//	Set RST signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetRST(BOOL ast)
{
	SetSignal(PIN_RST, ast);
}

//---------------------------------------------------------------------------
//
//	Get MSG signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetMSG()
{
	return GetSignal(PIN_MSG);
}

//---------------------------------------------------------------------------
//
//	Set MSG signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetMSG(BOOL ast)
{
	SetSignal(PIN_MSG, ast);
}

//---------------------------------------------------------------------------
//
//	Get CD signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetCD()
{
	return GetSignal(PIN_CD);
}

//---------------------------------------------------------------------------
//
//	Set CD Signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetCD(BOOL ast)
{
	SetSignal(PIN_CD, ast);
}

//---------------------------------------------------------------------------
//
//	Get IO Signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetIO()
{
	BOOL ast;
	ast = GetSignal(PIN_IO);

	if (actmode == INITIATOR) {
		// Change the data input/output direction by IO signal
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
//	Set IO signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetIO(BOOL ast)
{
	SetSignal(PIN_IO, ast);

	if (actmode == TARGET) {
		// Change the data input/output direction by IO signal
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
//	Get REQ signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetREQ()
{
	return GetSignal(PIN_REQ);
}

//---------------------------------------------------------------------------
//
//	Set REQ signal
//
//---------------------------------------------------------------------------
void GPIOBUS::SetREQ(BOOL ast)
{
	SetSignal(PIN_REQ, ast);
}

//---------------------------------------------------------------------------
//
// Get data signals
//
//---------------------------------------------------------------------------
BYTE GPIOBUS::GetDAT()
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
//	Set data signals
//
//---------------------------------------------------------------------------
void GPIOBUS::SetDAT(BYTE dat)
{
	// Write to port
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
//	Get data parity signal
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetDP()
{
	return GetSignal(PIN_DP);
}

//---------------------------------------------------------------------------
//
//	Receive command handshake
//
//---------------------------------------------------------------------------
int GPIOBUS::CommandHandShake(BYTE *buf)
{
	int i;
	BOOL ret;
	int count;

	// Only works in TARGET mode
	if (actmode != TARGET) {
		return 0;
	}

	// IRQs disabled
	DisableIRQ();

	// Get the first command byte
	i = 0;

	// Assert REQ signal
	SetSignal(PIN_REQ, ON);

	// Wait for ACK signal
	ret = WaitSignal(PIN_ACK, TRUE);

	// Wait until the signal line stabilizes
	SysTimer::SleepNsec(GPIO_DATA_SETTLING);

	// Get data
	*buf = GetDAT();

	// Disable REQ signal
	SetSignal(PIN_REQ, OFF);

	// Timeout waiting for ACK assertion
	if (!ret) {
		goto irq_enable_exit;
	}

	// Wait for ACK to clear
	ret = WaitSignal(PIN_ACK, FALSE);

	// Timeout waiting for ACK to clear
	if (!ret) {
		goto irq_enable_exit;
	}

	count = GetCommandByteCount(*buf);

	// Increment buffer pointer
	buf++;

	for (i = 1; i < count; i++) {
		// Assert REQ signal
		SetSignal(PIN_REQ, ON);

		// Wait for ACK signal
		ret = WaitSignal(PIN_ACK, TRUE);

		// Wait until the signal line stabilizes
		SysTimer::SleepNsec(GPIO_DATA_SETTLING);

		// Get data
		*buf = GetDAT();

		// Clear the REQ signal
		SetSignal(PIN_REQ, OFF);

		// Check for timeout waiting for ACK assertion
		if (!ret) {
			break;
		}

		// Wait for ACK to clear
		ret = WaitSignal(PIN_ACK, FALSE);

		// Check for timeout waiting for ACK to clear
		if (!ret) {
			break;
		}

		// Advance the buffer pointer to receive the next byte
		buf++;
	}

irq_enable_exit:
	// IRQs enabled
	EnableIRQ();

	// returned the number of bytes received
	return i;
}

//---------------------------------------------------------------------------
//
//	Data reception handshake
//
//---------------------------------------------------------------------------
int GPIOBUS::ReceiveHandShake(BYTE *buf, int count)
{
	int i;
	BOOL ret;
	DWORD phase;

	// Disable IRQs
	DisableIRQ();

	if (actmode == TARGET) {
		for (i = 0; i < count; i++) {
			// Assert the REQ signal
			SetSignal(PIN_REQ, ON);

			// Wait for ACK
			ret = WaitSignal(PIN_ACK, TRUE);

			// Wait until the signal line stabilizes
			SysTimer::SleepNsec(GPIO_DATA_SETTLING);

			// Get data
			*buf = GetDAT();

			// Clear the REQ signal
			SetSignal(PIN_REQ, OFF);

			// Check for timeout waiting for ACK signal
			if (!ret) {
				break;
			}

			// Wait for ACK to clear
			ret = WaitSignal(PIN_ACK, FALSE);

			// Check for timeout waiting for ACK to clear
			if (!ret) {
				break;
			}

			// Advance the buffer pointer to receive the next byte
			buf++;
		}
	} else {
		// Get phase
		phase = Aquire() & GPIO_MCI;

		for (i = 0; i < count; i++) {
			// Wait for the REQ signal to be asserted
			ret = WaitSignal(PIN_REQ, TRUE);

			// Check for timeout waiting for REQ signal
			if (!ret) {
				break;
			}

			// Phase error
			if ((signals & GPIO_MCI) != phase) {
				break;
			}

			// Wait until the signal line stabilizes
			SysTimer::SleepNsec(GPIO_DATA_SETTLING);

			// Get data
			*buf = GetDAT();

			// Assert the ACK signal
			SetSignal(PIN_ACK, ON);

			// Wait for REQ to clear
			ret = WaitSignal(PIN_REQ, FALSE);

			// Clear the ACK signal
			SetSignal(PIN_ACK, OFF);

			// Check for timeout waiting for REQ to clear
			if (!ret) {
				break;
			}

			// Phase error
			if ((signals & GPIO_MCI) != phase) {
				break;
			}

			// Advance the buffer pointer to receive the next byte
			buf++;
		}
	}

	// Re-enable IRQ
	EnableIRQ();

	// Return the number of bytes received
	return i;
}

//---------------------------------------------------------------------------
//
//	Data transmission handshake
//
//---------------------------------------------------------------------------
int GPIOBUS::SendHandShake(BYTE *buf, int count, int delay_after_bytes)
{
	int i;
	BOOL ret;
	DWORD phase;

	// Disable IRQs
	DisableIRQ();

	if (actmode == TARGET) {
		for (i = 0; i < count; i++) {
			if(i==delay_after_bytes){
				LOGTRACE("%s DELAYING for %dus after %d bytes", __PRETTY_FUNCTION__, SCSI_DELAY_SEND_DATA_DAYNAPORT_US, (int)delay_after_bytes);
				SysTimer::SleepUsec(SCSI_DELAY_SEND_DATA_DAYNAPORT_US);
			}

			// Set the DATA signals
			SetDAT(*buf);

			// Wait for ACK to clear
			ret = WaitSignal(PIN_ACK, FALSE);

			// Check for timeout waiting for ACK to clear
			if (!ret) {
				break;
			}

			// Already waiting for ACK to clear

			// Assert the REQ signal
			SetSignal(PIN_REQ, ON);

			// Wait for ACK
			ret = WaitSignal(PIN_ACK, TRUE);

			// Clear REQ signal
			SetSignal(PIN_REQ, OFF);

			// Check for timeout waiting for ACK to clear
			if (!ret) {
				break;
			}

			// Advance the data buffer pointer to receive the next byte
			buf++;
		}

		// Wait for ACK to clear
		WaitSignal(PIN_ACK, FALSE);
	} else {
		// Get Phase
		phase = Aquire() & GPIO_MCI;

		for (i = 0; i < count; i++) {
			if(i==delay_after_bytes){
				LOGTRACE("%s DELAYING for %dus after %d bytes", __PRETTY_FUNCTION__, SCSI_DELAY_SEND_DATA_DAYNAPORT_US, (int)delay_after_bytes);
				SysTimer::SleepUsec(SCSI_DELAY_SEND_DATA_DAYNAPORT_US);
			}

			// Set the DATA signals
			SetDAT(*buf);

			// Wait for REQ to be asserted
			ret = WaitSignal(PIN_REQ, TRUE);

			// Check for timeout waiting for REQ to be asserted
			if (!ret) {
				break;
			}

			// Phase error
			if ((signals & GPIO_MCI) != phase) {
				break;
			}

			// Already waiting for REQ assertion

			// Assert the ACK signal
			SetSignal(PIN_ACK, ON);

			// Wait for REQ to clear
			ret = WaitSignal(PIN_REQ, FALSE);

			// Clear the ACK signal
			SetSignal(PIN_ACK, OFF);

			// Check for timeout waiting for REQ to clear
			if (!ret) {
				break;
			}

			// Phase error
			if ((signals & GPIO_MCI) != phase) {
				break;
			}

			// Advance the data buffer pointer to receive the next byte
			buf++;
		}
	}

	// Re-enable IRQ
	EnableIRQ();

	// Return number of transmissions
	return i;
}

#ifdef USE_SEL_EVENT_ENABLE
//---------------------------------------------------------------------------
//
//	SEL signal event polling
//
//---------------------------------------------------------------------------
int GPIOBUS::PollSelectEvent()
{
	// clear errno
	errno = 0;
	struct epoll_event epev;
	struct gpioevent_data gpev;

	if (epoll_wait(epfd, &epev, 1, -1) <= 0) {
                LOGWARN("%s epoll_wait failed", __PRETTY_FUNCTION__);
		return -1;
	}

	if (read(selevreq.fd, &gpev, sizeof(gpev)) < 0) {
            LOGWARN("%s read failed", __PRETTY_FUNCTION__);
            return -1;
        }

	return 0;
}

//---------------------------------------------------------------------------
//
//	Cancel SEL signal event
//
//---------------------------------------------------------------------------
void GPIOBUS::ClearSelectEvent()
{
}
#endif	// USE_SEL_EVENT_ENABLE

//---------------------------------------------------------------------------
//
//	Signal table
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
//	Create work table
//
//---------------------------------------------------------------------------
void GPIOBUS::MakeTable(void)
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

	// Create parity table
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
	// Mask and setting data generation
	memset(tblDatMsk, 0xff, sizeof(tblDatMsk));
	memset(tblDatSet, 0x00, sizeof(tblDatSet));
	for (i = 0; i < 0x100; i++) {
		// Bit string for inspection
		bits = (DWORD)i;

		// Get parity
		if (tblParity[i]) {
			bits |= (1 << 8);
		}

		// Bit check
		for (j = 0; j < 9; j++) {
			// Index and shift amount calculation
			index = pintbl[j] / 10;
			shift = (pintbl[j] % 10) * 3;

			// Mask data
			tblDatMsk[index][i] &= ~(0x7 << shift);

			// Setting data
			if (bits & 1) {
				tblDatSet[index][i] |= (1 << shift);
			}

			bits >>= 1;
		}
	}
#else
	// Mask and setting data generation
	memset(tblDatMsk, 0x00, sizeof(tblDatMsk));
	memset(tblDatSet, 0x00, sizeof(tblDatSet));
	for (i = 0; i < 0x100; i++) {
		// bit string for inspection
		bits = (DWORD)i;

		// get parity
		if (tblParity[i]) {
			bits |= (1 << 8);
		}

#if SIGNAL_CONTROL_MODE == 1
		// Negative logic is inverted
		bits = ~bits;
#endif

		// Create GPIO register information
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
//	Control signal setting
//
//---------------------------------------------------------------------------
void GPIOBUS::SetControl(int pin, BOOL ast)
{
	PinSetSignal(pin, ast);
}

//---------------------------------------------------------------------------
//
//	Input/output mode setting
//
//---------------------------------------------------------------------------
void GPIOBUS::SetMode(int pin, int mode)
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
//	Get input signal value
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::GetSignal(int pin)
{
	return  (signals >> pin) & 1;
}

//---------------------------------------------------------------------------
//
//	Set output signal value
//
//---------------------------------------------------------------------------
void GPIOBUS::SetSignal(int pin, BOOL ast)
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
//	Wait for signal change
//
//---------------------------------------------------------------------------
BOOL GPIOBUS::WaitSignal(int pin, BOOL ast)
{
	DWORD now;
	DWORD timeout;

	// Get current time
	now = SysTimer::GetTimerLow();

	// Calculate timeout (3000ms)
	timeout = 3000 * 1000;

	// end immediately if the signal has changed
	do {
		// Immediately upon receiving a reset
		Aquire();
		if (GetRST()) {
			return FALSE;
		}

		// Check for the signal edge
        if (((signals >> pin) ^ ~ast) & 1) {
			return TRUE;
		}
	} while ((SysTimer::GetTimerLow() - now) < timeout);

	// We timed out waiting for the signal
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Disable IRQ
//
//---------------------------------------------------------------------------
void GPIOBUS::DisableIRQ()
{
	if (rpitype == 4) {
		// RPI4 is disabled by GICC
		giccpmr = gicc[GICC_PMR];
		gicc[GICC_PMR] = 0;
	} else if (rpitype == 2) {
		// RPI2,3 disable core timer IRQ
		tintcore = sched_getcpu() + QA7_CORE0_TINTC;
		tintctl = qa7regs[tintcore];
		qa7regs[tintcore] = 0;
	} else {
		// Stop system timer interrupt with interrupt controller
		irptenb = irpctl[IRPT_ENB_IRQ_1];
		irpctl[IRPT_DIS_IRQ_1] = irptenb & 0xf;
	}
}

//---------------------------------------------------------------------------
//
//	Enable IRQ
//
//---------------------------------------------------------------------------
void GPIOBUS::EnableIRQ()
{
	if (rpitype == 4) {
		// RPI4 enables interrupts via the GICC
		gicc[GICC_PMR] = giccpmr;
	} else if (rpitype == 2) {
		// RPI2,3 re-enable core timer IRQ
		qa7regs[tintcore] = tintctl;
	} else {
		// Restart the system timer interrupt with the interrupt controller
		irpctl[IRPT_ENB_IRQ_1] = irptenb & 0xf;
	}
}

//---------------------------------------------------------------------------
//
//	Pin direction setting (input/output)
//
//---------------------------------------------------------------------------
void GPIOBUS::PinConfig(int pin, int mode)
{
	int index;
	DWORD mask;

	// Check for invalid pin
	if (pin < 0) {
		return;
	}

	index = pin / 10;
	mask = ~(0x7 << ((pin % 10) * 3));
	gpio[index] = (gpio[index] & mask) | ((mode & 0x7) << ((pin % 10) * 3));
}

//---------------------------------------------------------------------------
//
//	Pin pull-up/pull-down setting
//
//---------------------------------------------------------------------------
void GPIOBUS::PullConfig(int pin, int mode)
{
	int shift;
	DWORD bits;
	DWORD pull;

	// Check for invalid pin
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
//	Set output pin
//
//---------------------------------------------------------------------------
void GPIOBUS::PinSetSignal(int pin, BOOL ast)
{
	// Check for invalid pin
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
//	Set the signal drive strength
//
//---------------------------------------------------------------------------
void GPIOBUS::DrvConfig(DWORD drive)
{
	DWORD data;

	data = pads[PAD_0_27];
	pads[PAD_0_27] = (0xFFFFFFF8 & data) | drive | 0x5a000000;
}


//---------------------------------------------------------------------------
//
//	Generic Phase Acquisition (Doesn't read GPIO)
//
//---------------------------------------------------------------------------
BUS::phase_t GPIOBUS::GetPhaseRaw(DWORD raw_data)
{
	DWORD mci;

	// Selection Phase
	if (GetPinRaw(raw_data, PIN_SEL)) 
	{
		if(GetPinRaw(raw_data, PIN_IO)){
			return BUS::reselection;
		}else{
			return BUS::selection;
		}
	}

	// Bus busy phase
	if (!GetPinRaw(raw_data, PIN_BSY)) {
		return BUS::busfree;
	}

	// Get target phase from bus signal line
	mci = GetPinRaw(raw_data, PIN_MSG) ? 0x04 : 0x00;
	mci |= GetPinRaw(raw_data, PIN_CD) ? 0x02 : 0x00;
	mci |= GetPinRaw(raw_data, PIN_IO) ? 0x01 : 0x00;
	return GetPhase(mci);
}

//---------------------------------------------------------------------------
//
//	Get the number of bytes for a command
//
// TODO The command length should be determined based on the bytes transferred in the COMMAND phase
//
//---------------------------------------------------------------------------
int GPIOBUS::GetCommandByteCount(BYTE opcode) {
	if (opcode == 0x88 || opcode == 0x8A || opcode == 0x8F || opcode == 0x9E) {
		return 16;
	}
	else if (opcode == 0xA0) {
		return 12;
	}
	else if (opcode >= 0x20 && opcode <= 0x7D) {
		return 10;
	} else {
		return 6;
	}
}

//---------------------------------------------------------------------------
//
//	System timer address
//
//---------------------------------------------------------------------------
volatile DWORD* SysTimer::systaddr;

//---------------------------------------------------------------------------
//
//	ARM timer address
//
//---------------------------------------------------------------------------
volatile DWORD* SysTimer::armtaddr;

//---------------------------------------------------------------------------
//
//	Core frequency
//
//---------------------------------------------------------------------------
volatile DWORD SysTimer::corefreq;

//---------------------------------------------------------------------------
//
//	Initialize the system timer
//
//---------------------------------------------------------------------------
void SysTimer::Init(DWORD *syst, DWORD *armt)
{
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

	// Save the base address
	systaddr = syst;
	armtaddr = armt;

	// Change the ARM timer to free run mode
	armtaddr[ARMT_CTRL] = 0x00000282;

	// Get the core frequency
	corefreq = 0;
	fd = open("/dev/vcio", O_RDONLY);
	if (fd >= 0) {
		ioctl(fd, _IOWR(100, 0, char *), maxclock);
		corefreq = maxclock[6] / 1000000;
	}
	close(fd);
}

//---------------------------------------------------------------------------
//
//	Get system timer low byte
//
//---------------------------------------------------------------------------
DWORD SysTimer::GetTimerLow() {
	return systaddr[SYST_CLO];
}

//---------------------------------------------------------------------------
//
//	Get system timer high byte
//
//---------------------------------------------------------------------------
DWORD SysTimer::GetTimerHigh() {
	return systaddr[SYST_CHI];
}

//---------------------------------------------------------------------------
//
//	Sleep in nanoseconds
//
//---------------------------------------------------------------------------
void SysTimer::SleepNsec(DWORD nsec)
{
	DWORD diff;
	DWORD start;

	// If time is 0, don't do anything
	if (nsec == 0) {
		return;
	}

	// Calculate the timer difference
	diff = corefreq * nsec / 1000;

	// Return if the difference in time is too small
	if (diff == 0) {
		return;
	}

	// Start
	start = armtaddr[ARMT_FREERUN];

	// Loop until timer has elapsed
	while ((armtaddr[ARMT_FREERUN] - start) < diff);
}

//---------------------------------------------------------------------------
//
//	Sleep in microseconds
//
//---------------------------------------------------------------------------
void SysTimer::SleepUsec(DWORD usec)
{
	DWORD now;

	// If time is 0, don't do anything
	if (usec == 0) {
		return;
	}

	now = GetTimerLow();
	while ((GetTimerLow() - now) < usec);
}
