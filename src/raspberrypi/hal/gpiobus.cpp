//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	[ GPIO-SCSI bus ]
//
//---------------------------------------------------------------------------

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include "os.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include "config.h"
#include "log.h"
#include <array>
#ifdef __linux
#include <sys/epoll.h>
#endif

using namespace std;

#ifdef __linux
//---------------------------------------------------------------------------
//
//	imported from bcm_host.c
//
//---------------------------------------------------------------------------
static uint32_t get_dt_ranges(const char *filename, DWORD offset)
{
	uint32_t address = ~0;
	if (FILE *fp = fopen(filename, "rb"); fp) {
		fseek(fp, offset, SEEK_SET);
		if (array<BYTE, 4> buf; fread(buf.data(), 1, buf.size(), fp) == buf.size()) {
			address = (int)buf[0] << 24 | (int)buf[1] << 16 | (int)buf[2] << 8 | (int)buf[3] << 0;
		}
		fclose(fp);
	}
	return address;
}

uint32_t bcm_host_get_peripheral_address()
{
	uint32_t address = get_dt_ranges("/proc/device-tree/soc/ranges", 4);
	if (address == 0) {
		address = get_dt_ranges("/proc/device-tree/soc/ranges", 8);
	}
	address = (address == (uint32_t)~0) ? 0x20000000 : address;
	return address;
}
#endif

#ifdef __NetBSD__
// Assume the Raspberry Pi series and estimate the address from CPU
uint32_t bcm_host_get_peripheral_address()
{
	array<char, 1024> buf;
	size_t len = buf.size();
	DWORD address;

	if (sysctlbyname("hw.model", buf.data(), &len, NULL, 0) ||
	    strstr(buf, "ARM1176JZ-S") != buf.data()) {
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
#endif

bool GPIOBUS::Init(mode_e mode)
{
	// Save operation mode
	actmode = mode;

#if defined(__x86_64__) || defined(__X86__)
	return true;
#else
	int i;
#ifdef USE_SEL_EVENT_ENABLE
	epoll_event ev = {};
#endif

	// Get the base address
	baseaddr = (uint32_t)bcm_host_get_peripheral_address();

	// Open /dev/mem
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd == -1) {
        LOGERROR("Error: Unable to open /dev/mem. Are you running as root?")
		return false;
	}

	// Map peripheral region memory
	void *map = mmap(NULL, 0x1000100, PROT_READ | PROT_WRITE, MAP_SHARED, fd, baseaddr);
	if (map == MAP_FAILED) {
        LOGERROR("Error: Unable to map memory")
		close(fd);
		return false;
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
			return false;
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
	int pullmode = GPIO_PULLNONE;
#elif SIGNAL_CONTROL_MODE == 1
	int pullmode = GPIO_PULLUP;
#else
	int pullmode = GPIO_PULLDOWN;
#endif

	// Initialize all signals
	for (i = 0; SignalTable[i] >= 0; i++) {
		int j = SignalTable[i];
		PinSetSignal(j, OFF);
		PinConfig(j, GPIO_INPUT);
		PullConfig(j, pullmode);
	}

	// Set control signals
	PinSetSignal(PIN_ACT, OFF);
	PinSetSignal(PIN_TAD, OFF);
	PinSetSignal(PIN_IND, OFF);
	PinSetSignal(PIN_DTD, OFF);
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
		return false;
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
		return false;
	}

	// Close GPIO chip file handle
	close(fd);

	// epoll initialization
	epfd = epoll_create(1);
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

	return true;
#endif // ifdef __x86_64__ || __X86__

}

void GPIOBUS::Cleanup()
{
#if defined(__x86_64__) || defined(__X86__)
	return;
#else
	// Release SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
	close(selevreq.fd);
#endif	// USE_SEL_EVENT_ENABLE

	// Set control signals
	PinSetSignal(PIN_ENB, OFF);
	PinSetSignal(PIN_ACT, OFF);
	PinSetSignal(PIN_TAD, OFF);
	PinSetSignal(PIN_IND, OFF);
	PinSetSignal(PIN_DTD, OFF);
	PinConfig(PIN_ACT, GPIO_INPUT);
	PinConfig(PIN_TAD, GPIO_INPUT);
	PinConfig(PIN_IND, GPIO_INPUT);
	PinConfig(PIN_DTD, GPIO_INPUT);

	// Initialize all signals
	for (int i = 0; SignalTable[i] >= 0; i++) {
		int pin = SignalTable[i];
		PinSetSignal(pin, OFF);
		PinConfig(pin, GPIO_INPUT);
		PullConfig(pin, GPIO_PULLNONE);
	}

	// Set drive strength back to 8mA
	DrvConfig(3);
#endif // ifdef __x86_64__ || __X86__
}

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

	if (actmode == mode_e::TARGET) {
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

		// Set the data bus signals to output
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

void GPIOBUS::SetENB(bool ast)
{
	PinSetSignal(PIN_ENB, ast ? ENB_ON : ENB_OFF);
}

bool GPIOBUS::GetBSY() const
{
	return GetSignal(PIN_BSY);
}

void GPIOBUS::SetBSY(bool ast)
{
	// Set BSY signal
	SetSignal(PIN_BSY, ast);

	if (actmode == mode_e::TARGET) {
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

bool GPIOBUS::GetSEL() const
{
	return GetSignal(PIN_SEL);
}

void GPIOBUS::SetSEL(bool ast)
{
	if (actmode == mode_e::INITIATOR && ast) {
		// Turn on ACTIVE signal
		SetControl(PIN_ACT, ACT_ON);
	}

	// Set SEL signal
	SetSignal(PIN_SEL, ast);
}

bool GPIOBUS::GetATN() const
{
	return GetSignal(PIN_ATN);
}

void GPIOBUS::SetATN(bool ast)
{
	SetSignal(PIN_ATN, ast);
}

bool GPIOBUS::GetACK() const
{
	return GetSignal(PIN_ACK);
}

void GPIOBUS::SetACK(bool ast)
{
	SetSignal(PIN_ACK, ast);
}

bool GPIOBUS::GetACT() const
{
	return GetSignal(PIN_ACT);
}

void GPIOBUS::SetACT(bool ast)
{
	SetSignal(PIN_ACT, ast);
}

bool GPIOBUS::GetRST() const
{
	return GetSignal(PIN_RST);
}

void GPIOBUS::SetRST(bool ast)
{
	SetSignal(PIN_RST, ast);
}

bool GPIOBUS::GetMSG() const
{
	return GetSignal(PIN_MSG);
}

void GPIOBUS::SetMSG(bool ast)
{
	SetSignal(PIN_MSG, ast);
}

bool GPIOBUS::GetCD() const
{
	return GetSignal(PIN_CD);
}

void GPIOBUS::SetCD(bool ast)
{
	SetSignal(PIN_CD, ast);
}

bool GPIOBUS::GetIO()
{
	bool ast = GetSignal(PIN_IO);

	if (actmode == mode_e::INITIATOR) {
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

void GPIOBUS::SetIO(bool ast)
{
	SetSignal(PIN_IO, ast);

	if (actmode == mode_e::TARGET) {
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

bool GPIOBUS::GetREQ() const
{
	return GetSignal(PIN_REQ);
}

void GPIOBUS::SetREQ(bool ast)
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
	uint32_t data = Acquire();
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
	DWORD fsel = gpfsel[0];
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

bool GPIOBUS::GetDP() const
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
	// Only works in TARGET mode
	if (actmode != mode_e::TARGET) {
		return 0;
	}

	DisableIRQ();

	// Assert REQ signal
	SetSignal(PIN_REQ, ON);

	// Wait for ACK signal
	bool ret = WaitSignal(PIN_ACK, ON);

	// Wait until the signal line stabilizes
	SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

	// Get data
	*buf = GetDAT();

	// Disable REQ signal
	SetSignal(PIN_REQ, OFF);

	// Timeout waiting for ACK assertion
	if (!ret) {
		EnableIRQ();
		return 0;
	}

	// Wait for ACK to clear
	ret = WaitSignal(PIN_ACK, OFF);

	// Timeout waiting for ACK to clear
	if (!ret) {
		EnableIRQ();
		return 0;
	}

	// The ICD AdSCSI ST, AdSCSI Plus ST and AdSCSI Micro ST host adapters allow SCSI devices to be connected
	// to the ACSI bus of Atari ST/TT computers and some clones. ICD-aware drivers prepend a $1F byte in front
	// of the CDB (effectively resulting in a custom SCSI command) in order to get access to the full SCSI
	// command set. Native ACSI is limited to the low SCSI command classes with command bytes < $20.
	// Most other host adapters (e.g. LINK96/97 and the one by Inventronik) and also several devices (e.g.
	// UltraSatan or GigaFile) that can directly be connected to the Atari's ACSI port also support ICD
	// semantics. I fact, these semantics have become a standard in the Atari world.

	// RaSCSI becomes ICD compatible by ignoring the prepended $1F byte before processing the CDB.
	if (*buf == 0x1F) {
		SetSignal(PIN_REQ, ON);

		ret = WaitSignal(PIN_ACK, ON);

		SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

		// Get the actual SCSI command
		*buf = GetDAT();

		SetSignal(PIN_REQ, OFF);

		if (!ret) {
			EnableIRQ();
			return 0;
		}

		WaitSignal(PIN_ACK, OFF);

		if (!ret) {
			EnableIRQ();
			return 0;
		}
	}

	int command_byte_count = GetCommandByteCount(*buf);

	// Increment buffer pointer
	buf++;

	int bytes_received;
	for (bytes_received = 1; bytes_received < command_byte_count; bytes_received++) {
		// Assert REQ signal
		SetSignal(PIN_REQ, ON);

		// Wait for ACK signal
		ret = WaitSignal(PIN_ACK, ON);

		// Wait until the signal line stabilizes
		SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

		// Get data
		*buf = GetDAT();

		// Clear the REQ signal
		SetSignal(PIN_REQ, OFF);

		// Check for timeout waiting for ACK assertion
		if (!ret) {
			break;
		}

		// Wait for ACK to clear
		ret = WaitSignal(PIN_ACK, OFF);

		// Check for timeout waiting for ACK to clear
		if (!ret) {
			break;
		}

		// Advance the buffer pointer to receive the next byte
		buf++;
	}

	EnableIRQ();

	return bytes_received;
}

//---------------------------------------------------------------------------
//
//	Data reception handshake
//
//---------------------------------------------------------------------------
int GPIOBUS::ReceiveHandShake(BYTE *buf, int count)
{
	int i;

	// Disable IRQs
	DisableIRQ();

	if (actmode == mode_e::TARGET) {
		for (i = 0; i < count; i++) {
			// Assert the REQ signal
			SetSignal(PIN_REQ, ON);

			// Wait for ACK
			bool ret = WaitSignal(PIN_ACK, ON);

			// Wait until the signal line stabilizes
			SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

			// Get data
			*buf = GetDAT();

			// Clear the REQ signal
			SetSignal(PIN_REQ, OFF);

			// Check for timeout waiting for ACK signal
			if (!ret) {
				break;
			}

			// Wait for ACK to clear
			ret = WaitSignal(PIN_ACK, OFF);

			// Check for timeout waiting for ACK to clear
			if (!ret) {
				break;
			}

			// Advance the buffer pointer to receive the next byte
			buf++;
		}
	} else {
		// Get phase
		DWORD phase = Acquire() & GPIO_MCI;

		for (i = 0; i < count; i++) {
			// Wait for the REQ signal to be asserted
			bool ret = WaitSignal(PIN_REQ, ON);

			// Check for timeout waiting for REQ signal
			if (!ret) {
				break;
			}

			// Phase error
			if ((signals & GPIO_MCI) != phase) {
				break;
			}

			// Wait until the signal line stabilizes
			SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

			// Get data
			*buf = GetDAT();

			// Assert the ACK signal
			SetSignal(PIN_ACK, ON);

			// Wait for REQ to clear
			ret = WaitSignal(PIN_REQ, OFF);

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

	// Disable IRQs
	DisableIRQ();

	if (actmode == mode_e::TARGET) {
		for (i = 0; i < count; i++) {
			if(i==delay_after_bytes){
				LOGTRACE("%s DELAYING for %dus after %d bytes", __PRETTY_FUNCTION__, SCSI_DELAY_SEND_DATA_DAYNAPORT_US, (int)delay_after_bytes)
				SysTimer::SleepUsec(SCSI_DELAY_SEND_DATA_DAYNAPORT_US);
			}

			// Set the DATA signals
			SetDAT(*buf);

			// Wait for ACK to clear
			bool ret = WaitSignal(PIN_ACK, OFF);

			// Check for timeout waiting for ACK to clear
			if (!ret) {
				break;
			}

			// Already waiting for ACK to clear

			// Assert the REQ signal
			SetSignal(PIN_REQ, ON);

			// Wait for ACK
			ret = WaitSignal(PIN_ACK, ON);

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
		WaitSignal(PIN_ACK, OFF);
	} else {
		// Get Phase
		DWORD phase = Acquire() & GPIO_MCI;

		for (i = 0; i < count; i++) {
			if(i==delay_after_bytes){
				LOGTRACE("%s DELAYING for %dus after %d bytes", __PRETTY_FUNCTION__, SCSI_DELAY_SEND_DATA_DAYNAPORT_US, (int)delay_after_bytes)
				SysTimer::SleepUsec(SCSI_DELAY_SEND_DATA_DAYNAPORT_US);
			}

			// Set the DATA signals
			SetDAT(*buf);

			// Wait for REQ to be asserted
			bool ret = WaitSignal(PIN_REQ, ON);

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
			ret = WaitSignal(PIN_REQ, OFF);

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
bool GPIOBUS::PollSelectEvent()
{
	errno = 0;

	if (epoll_event epev; epoll_wait(epfd, &epev, 1, -1) <= 0) {
		LOGWARN("%s epoll_wait failed", __PRETTY_FUNCTION__)
		return false;
	}

	if (gpioevent_data gpev; read(selevreq.fd, &gpev, sizeof(gpev)) < 0) {
		LOGWARN("%s read failed", __PRETTY_FUNCTION__)
        return false;
	}

	return true;
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
const array<int, 19> GPIOBUS::SignalTable = {
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
	const array<int, 9> pintbl = {
		PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4,
		PIN_DT5, PIN_DT6, PIN_DT7, PIN_DP
	};

	array<bool, 256> tblParity;

	// Create parity table
	for (uint32_t i = 0; i < 0x100; i++) {
		uint32_t bits = i;
		uint32_t parity = 0;
		for (int j = 0; j < 8; j++) {
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
	for (uint32_t i = 0; i < 0x100; i++) {
		// Bit string for inspection
		uint32_t bits = i;

		// Get parity
		if (tblParity[i]) {
			bits |= (1 << 8);
		}

		// Bit check
		for (int j = 0; j < 9; j++) {
			// Index and shift amount calculation
			int index = pintbl[j] / 10;
			int shift = (pintbl[j] % 10) * 3;

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
	fill(tblDatMsk.begin(), tblDatMsk.end(), 0x00);
	fill(tblDatSet.begin(), tblDatSet.end(), 0x00);
	for (uint32_t i = 0; i < 0x100; i++) {
		// bit string for inspection
		uint32_t bits = i;

		// get parity
		if (tblParity[i]) {
			bits |= (1 << 8);
		}

#if SIGNAL_CONTROL_MODE == 1
		// Negative logic is inverted
		bits = ~bits;
#endif

		// Create GPIO register information
		uint32_t gpclr = 0;
		uint32_t gpset = 0;
		for (int j = 0; j < 9; j++) {
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
void GPIOBUS::SetControl(int pin, bool ast)
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
#if SIGNAL_CONTROL_MODE == 0
	if (mode == OUT) {
		return;
	}
#endif	// SIGNAL_CONTROL_MODE

	int index = pin / 10;
	int shift = (pin % 10) * 3;
	DWORD data = gpfsel[index];
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
bool GPIOBUS::GetSignal(int pin) const
{
	return (signals >> pin) & 1;
}

//---------------------------------------------------------------------------
//
//	Set output signal value
//
//---------------------------------------------------------------------------
void GPIOBUS::SetSignal(int pin, bool ast)
{
#if SIGNAL_CONTROL_MODE == 0
	int index = pin / 10;
	int shift = (pin % 10) * 3;
	DWORD data = gpfsel[index];
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
bool GPIOBUS::WaitSignal(int pin, int ast)
{
	// Get current time
	uint32_t now = SysTimer::GetTimerLow();

	// Calculate timeout (3000ms)
	uint32_t timeout = 3000 * 1000;

	// end immediately if the signal has changed
	do {
		// Immediately upon receiving a reset
		Acquire();
		if (GetRST()) {
			return false;
		}

		// Check for the signal edge
        if (((signals >> pin) ^ ~ast) & 1) {
			return true;
		}
	} while ((SysTimer::GetTimerLow() - now) < timeout);

	// We timed out waiting for the signal
	return false;
}

void GPIOBUS::DisableIRQ()
{
#ifdef __linux
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
#else
	(void)0;
#endif
}

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
	// Check for invalid pin
	if (pin < 0) {
		return;
	}

	int index = pin / 10;
	DWORD mask = ~(0x7 << ((pin % 10) * 3));
	gpio[index] = (gpio[index] & mask) | ((mode & 0x7) << ((pin % 10) * 3));
}

//---------------------------------------------------------------------------
//
//	Pin pull-up/pull-down setting
//
//---------------------------------------------------------------------------
void GPIOBUS::PullConfig(int pin, int mode)
{
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
		int shift = (pin & 0xf) << 1;
		DWORD bits = gpio[GPIO_PUPPDN0 + (pin >> 4)];
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
void GPIOBUS::PinSetSignal(int pin, bool ast)
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
	DWORD data = pads[PAD_0_27];
	pads[PAD_0_27] = (0xFFFFFFF8 & data) | drive | 0x5a000000;
}


//---------------------------------------------------------------------------
//
//	Generic Phase Acquisition (Doesn't read GPIO)
//
//---------------------------------------------------------------------------
BUS::phase_t GPIOBUS::GetPhaseRaw(DWORD raw_data)
{
	// Selection Phase
	if (GetPinRaw(raw_data, PIN_SEL)) {
		if(GetPinRaw(raw_data, PIN_IO)) {
			return BUS::phase_t::reselection;
		} else{
			return BUS::phase_t::selection;
		}
	}

	// Bus busy phase
	if (!GetPinRaw(raw_data, PIN_BSY)) {
		return BUS::phase_t::busfree;
	}

	// Get target phase from bus signal line
	int mci = GetPinRaw(raw_data, PIN_MSG) ? 0x04 : 0x00;
	mci |= GetPinRaw(raw_data, PIN_CD) ? 0x02 : 0x00;
	mci |= GetPinRaw(raw_data, PIN_IO) ? 0x01 : 0x00;
	return GetPhase(mci);
}

//---------------------------------------------------------------------------
//
// Get the number of bytes for a command
//
//---------------------------------------------------------------------------
int GPIOBUS::GetCommandByteCount(BYTE opcode) {
	if (opcode == 0x88 || opcode == 0x8A || opcode == 0x8F || opcode == 0x91 || opcode == 0x9E || opcode == 0x9F) {
		return 16;
	}
	else if (opcode == 0xA0) {
		return 12;
	}
	else if (opcode == 0x05 || (opcode >= 0x20 && opcode <= 0x7D)) {
		return 10;
	} else {
		return 6;
	}
}

