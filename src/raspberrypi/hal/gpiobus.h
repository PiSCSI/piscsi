//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ GPIO-SCSI bus ]
//
//---------------------------------------------------------------------------

#pragma once

#include "config.h"
#include "scsi.h"
#include <array>

#ifdef __linux
#include <linux/gpio.h>
#endif

//---------------------------------------------------------------------------
//
//	Connection method definitions
//
//---------------------------------------------------------------------------
//#define CONNECT_TYPE_STANDARD		// Standard (SCSI logic, standard pin assignment)
//#define CONNECT_TYPE_FULLSPEC		// Full spec (SCSI logic, standard pin assignment)
//#define CONNECT_TYPE_AIBOM		// AIBOM version (positive logic, unique pin assignment)
//#define CONNECT_TYPE_GAMERNIUM	// GAMERnium.com version (standard logic, unique pin assignment)

#if defined CONNECT_TYPE_STANDARD
#include "hal/gpiobus_standard.h"
#elif defined CONNECT_TYPE_FULLSPEC
#include "hal/gpiobus_fullspec.h"
#elif defined CONNECT_TYPE_AIBOM
#include "hal/gpiobus_aibom.h"
#elif defined CONNECT_TYPE_GAMERNIUM
#include "hal/gpiobus_gamernium.h"
#else
#error Invalid connection type or none specified
#endif

using namespace std; //NOSONAR Not relevant for rascsi

//---------------------------------------------------------------------------
//
//	Signal control logic and pin assignment customization
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	SIGNAL_CONTROL_MODE: Signal control mode selection
//	 You can customize the signal control logic from Version 1.22
//
//	 0:SCSI logical specification
//	    Conversion board using 74LS641-1 etc. directly connected or published on HP
//	  True  : 0V
//	  False : Open collector output (disconnect from bus)
//
//	 1:Negative logic specification (when using conversion board for negative logic -> SCSI logic)
//	    There is no conversion board with this specification at this time
//	  True  : 0V   -> (CONVERT) -> 0V
//	  False : 3.3V -> (CONVERT) -> Open collector output
//
//	 2:Positive logic specification (when using the conversion board for positive logic -> SCSI logic)
//	    RaSCSI Adapter Rev.C @132sync etc.
//
//	  True  : 3.3V -> (CONVERT) -> 0V
//	  False : 0V   -> (CONVERT) -> Open collector output
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	Control signal pin assignment setting
//	 GPIO pin mapping table for control signals.
//
//	 Control signal:
//	  PIN_ACT
//	    Signal that indicates the status of processing SCSI command.
//	  PIN_ENB
//	    Signal that indicates the valid signal from start to finish.
//	  PIN_TAD
//	    Signal that indicates the input/output direction of the target signal (BSY,IO,CD,MSG,REG).
//	  PIN_IND
//	    Signal that indicates the input/output direction of the initiator signal (SEL, ATN, RST, ACK).
//	  PIN_DTD
//	    Signal that indicates the input/output direction of the data lines (DT0...DT7,DP).
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	Control signal output logic
//	  0V:FALSE  3.3V:TRUE
//
//	  ACT_ON
//	    PIN_ACT signal
//	  ENB_ON
//	    PIN_ENB signal
//	  TAD_IN
//	    PIN_TAD This is the logic when inputting.
//	  IND_IN
//	    PIN_ENB This is the logic when inputting.
//    DTD_IN
//	    PIN_ENB This is the logic when inputting.
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	SCSI signal pin assignment setting
//	  GPIO pin mapping table for SCSI signals.
//	  PIN_DT0～PIN_SEL
//
//---------------------------------------------------------------------------

#define ALL_SCSI_PINS \
    ((1<<PIN_DT0)|\
    (1<<PIN_DT1)|\
    (1<<PIN_DT2)|\
    (1<<PIN_DT3)|\
    (1<<PIN_DT4)|\
    (1<<PIN_DT5)|\
    (1<<PIN_DT6)|\
    (1<<PIN_DT7)|\
    (1<<PIN_DP)|\
    (1<<PIN_ATN)|\
    (1<<PIN_RST)|\
    (1<<PIN_ACK)|\
    (1<<PIN_REQ)|\
    (1<<PIN_MSG)|\
    (1<<PIN_CD)|\
    (1<<PIN_IO)|\
    (1<<PIN_BSY)|\
    (1<<PIN_SEL))

//---------------------------------------------------------------------------
//
//	Constant declarations (GPIO)
//
//---------------------------------------------------------------------------
const static uint32_t SYST_OFFSET =		0x00003000;
const static uint32_t IRPT_OFFSET =		0x0000B200;
const static uint32_t ARMT_OFFSET =		0x0000B400;
const static uint32_t PADS_OFFSET =		0x00100000;
const static uint32_t GPIO_OFFSET =		0x00200000;
const static uint32_t QA7_OFFSET =		0x01000000;

const static int GPIO_INPUT	=		0;
const static int GPIO_OUTPUT =		1;
const static int GPIO_PULLNONE =	0;
const static int GPIO_PULLDOWN =	1;
const static int GPIO_PULLUP =		2;
const static int GPIO_FSEL_0 =		0;
const static int GPIO_FSEL_1 =		1;
const static int GPIO_FSEL_2 =		2;
const static int GPIO_FSEL_3 =		3;
const static int GPIO_SET_0 =		7;
const static int GPIO_CLR_0 =		10;
const static int GPIO_LEV_0 =		13;
const static int GPIO_EDS_0 =		16;
const static int GPIO_REN_0 =		19;
const static int GPIO_FEN_0 =		22;
const static int GPIO_HEN_0 =		25;
const static int GPIO_LEN_0 =		28;
const static int GPIO_AREN_0 =		31;
const static int GPIO_AFEN_0 =		34;
const static int GPIO_PUD =			37;
const static int GPIO_CLK_0 =		38;
const static int GPIO_GPPINMUXSD =	52;
const static int GPIO_PUPPDN0 =		57;
const static int GPIO_PUPPDN1 =		58;
const static int GPIO_PUPPDN3 =		59;
const static int GPIO_PUPPDN4 =		60;
const static int PAD_0_27 =			11;
const static int SYST_CS =			0;
const static int SYST_CLO =			1;
const static int SYST_CHI =			2;
const static int SYST_C0 =			3;
const static int SYST_C1 =			4;
const static int SYST_C2 =			5;
const static int SYST_C3 =			6;
const static int ARMT_LOAD =		0;
const static int ARMT_VALUE =		1;
const static int ARMT_CTRL =		2;
const static int ARMT_CLRIRQ =		3;
const static int ARMT_RAWIRQ =		4;
const static int ARMT_MSKIRQ =		5;
const static int ARMT_RELOAD =		6;
const static int ARMT_PREDIV =		7;
const static int ARMT_FREERUN =		8;
const static int IRPT_PND_IRQ_B =	0;
const static int IRPT_PND_IRQ_1 =	1;
const static int IRPT_PND_IRQ_2 =	2;
const static int IRPT_FIQ_CNTL =	3;
const static int IRPT_ENB_IRQ_1 =	4;
const static int IRPT_ENB_IRQ_2 =	5;
const static int IRPT_ENB_IRQ_B =	6;
const static int IRPT_DIS_IRQ_1 =	7;
const static int IRPT_DIS_IRQ_2 =	8;
const static int IRPT_DIS_IRQ_B =	9;
const static int QA7_CORE0_TINTC = 	16;
const static int GPIO_IRQ =			(32 + 20);	// GPIO3

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
//	Constant declarations (GIC)
//
//---------------------------------------------------------------------------
const static uint32_t ARM_GICD_BASE =	0xFF841000;
const static uint32_t ARM_GICC_BASE =	0xFF842000;
const static uint32_t ARM_GIC_END =		0xFF847FFF;
const static int GICD_CTLR =		0x000;
const static int GICD_IGROUPR0 =	0x020;
const static int GICD_ISENABLER0 =	0x040;
const static int GICD_ICENABLER0 =	0x060;
const static int GICD_ISPENDR0 =	0x080;
const static int GICD_ICPENDR0 =	0x0A0;
const static int GICD_ISACTIVER0 =	0x0C0;
const static int GICD_ICACTIVER0 =	0x0E0;
const static int GICD_IPRIORITYR0 =	0x100;
const static int GICD_ITARGETSR0 =	0x200;
const static int GICD_ICFGR0 =		0x300;
const static int GICD_SGIR =		0x3C0;
const static int GICC_CTLR =		0x000;
const static int GICC_PMR =			0x001;
const static int GICC_IAR =			0x003;
const static int GICC_EOIR =		0x004;

//---------------------------------------------------------------------------
//
//	Constant declarations (GIC IRQ)
//
//---------------------------------------------------------------------------
const static int GIC_IRQLOCAL0 =	(16 + 14);
const static int GIC_GPIO_IRQ =		(32 + 116);	// GPIO3

//---------------------------------------------------------------------------
//
//	Constant declarations (Control signals)
//
//---------------------------------------------------------------------------
#define ACT_OFF	!ACT_ON
#define ENB_OFF	!ENB_ON
#define TAD_OUT	!TAD_IN
#define IND_OUT	!IND_IN
#define DTD_OUT	!DTD_IN

//---------------------------------------------------------------------------
//
//	Constant declarations (SCSI)
//
//---------------------------------------------------------------------------
#define IN		GPIO_INPUT
#define OUT		GPIO_OUTPUT
const static int ON =	1;
const static int OFF =	0;

//---------------------------------------------------------------------------
//
//	Constant declarations (bus control timing)
//
//---------------------------------------------------------------------------
// SCSI Bus timings taken from:
//     https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-05.html
const static int SCSI_DELAY_ARBITRATION_DELAY_NS =        2400;
const static int SCSI_DELAY_ASSERTION_PERIOD_NS =           90;
const static int SCSI_DELAY_BUS_CLEAR_DELAY_NS =           800;
const static int SCSI_DELAY_BUS_FREE_DELAY_NS =            800;
const static int SCSI_DELAY_BUS_SET_DELAY_NS =            1800;
const static int SCSI_DELAY_BUS_SETTLE_DELAY_NS =          400;
const static int SCSI_DELAY_CABLE_SKEW_DELAY_NS =           10;
const static int SCSI_DELAY_DATA_RELEASE_DELAY_NS =        400;
const static int SCSI_DELAY_DESKEW_DELAY_NS =               45;
const static int SCSI_DELAY_DISCONNECTION_DELAY_US =       200;
const static int SCSI_DELAY_HOLD_TIME_NS =                  45;
const static int SCSI_DELAY_NEGATION_PERIOD_NS =            90;
const static int SCSI_DELAY_POWER_ON_TO_SELECTION_TIME_S =  10;      // (recommended)
const static int SCSI_DELAY_RESET_TO_SELECTION_TIME_US =   250*1000; // (recommended)
const static int SCSI_DELAY_RESET_HOLD_TIME_US =            25;
const static int SCSI_DELAY_SELECTION_ABORT_TIME_US =      200;
const static int SCSI_DELAY_SELECTION_TIMEOUT_DELAY_NS =  250*1000;  // (recommended)
const static int SCSI_DELAY_FAST_ASSERTION_PERIOD_NS =      30;
const static int SCSI_DELAY_FAST_CABLE_SKEW_DELAY_NS =       5;
const static int SCSI_DELAY_FAST_DESKEW_DELAY_NS =          20;
const static int SCSI_DELAY_FAST_HOLD_TIME_NS =             10;
const static int SCSI_DELAY_FAST_NEGATION_PERIOD_NS =       30;

// The DaynaPort SCSI Link do a short delay in the middle of transfering
// a packet. This is the number of uS that will be delayed between the
// header and the actual data.
const static int SCSI_DELAY_SEND_DATA_DAYNAPORT_US = 100;

//---------------------------------------------------------------------------
//
//	Class definition
//
//---------------------------------------------------------------------------
class GPIOBUS final : public BUS
{
public:
	// Basic Functions
	GPIOBUS()= default;
	~GPIOBUS() override = default;
										// Destructor
	bool Init(mode_e mode = mode_e::TARGET) override;
										// Initialization
	void Reset() override;
										// Reset
	void Cleanup() override;
										// Cleanup

	//---------------------------------------------------------------------------
	//
	//	Bus signal acquisition
	//
	//---------------------------------------------------------------------------
	inline uint32_t Acquire() override
	{
	#if defined(__x86_64__) || defined(__X86__)
		// Only used for development/debugging purposes. Isn't really applicable
		// to any real-world RaSCSI application
		return 0;
	#else
		signals = *level;

	#if SIGNAL_CONTROL_MODE < 2
		// Invert if negative logic (internal processing is unified to positive logic)
		signals = ~signals;
	#endif	// SIGNAL_CONTROL_MODE

		return signals;
	#endif // ifdef __x86_64__ || __X86__
	}

	void SetENB(bool ast);
										// Set ENB signal

	bool GetBSY() const override;
										// Get BSY signal
	void SetBSY(bool ast) override;
										// Set BSY signal

	bool GetSEL() const override;
										// Get SEL signal
	void SetSEL(bool ast) override;
										// Set SEL signal

	bool GetATN() const override;
										// Get ATN signal
	void SetATN(bool ast) override;
										// Set ATN signal

	bool GetACK() const override;
										// Get ACK signal
	void SetACK(bool ast) override;
										// Set ACK signal

	bool GetACT() const;
										// Get ACT signal
	void SetACT(bool ast);
										// Set ACT signal

	bool GetRST() const override;
										// Get RST signal
	void SetRST(bool ast) override;
										// Set RST signal

	bool GetMSG() const override;
										// Get MSG signal
	void SetMSG(bool ast) override;
										// Set MSG signal

	bool GetCD() const override;
										// Get CD signal
	void SetCD(bool ast) override;
										// Set CD signal

	bool GetIO() override;
										// Get IO signal
	void SetIO(bool ast) override;
										// Set IO signal

	bool GetREQ() const override;
										// Get REQ signal
	void SetREQ(bool ast) override;
										// Set REQ signal

	BYTE GetDAT() override;
										// Get DAT signal
	void SetDAT(BYTE dat) override;
										// Set DAT signal
	bool GetDP() const override;
										// Get Data parity signal
	int CommandHandShake(BYTE *buf) override;
										// Command receive handshake
	int ReceiveHandShake(BYTE *buf, int count) override;
										// Data receive handshake
	int SendHandShake(BYTE *buf, int count, int delay_after_bytes) override;
										// Data transmission handshake

	static BUS::phase_t GetPhaseRaw(DWORD raw_data);
										// Get the phase based on raw data

	static int GetCommandByteCount(BYTE opcode);

	#ifdef USE_SEL_EVENT_ENABLE
	// SEL signal interrupt
	bool PollSelectEvent();
										// SEL signal event polling
	void ClearSelectEvent();
										// Clear SEL signal event
#endif	// USE_SEL_EVENT_ENABLE

private:
	// SCSI I/O signal control
	void MakeTable();
										// Create work data
	void SetControl(int pin, bool ast);
										// Set Control Signal
	void SetMode(int pin, int mode);
										// Set SCSI I/O mode
	bool GetSignal(int pin) const override;
										// Get SCSI input signal value
	void SetSignal(int pin, bool ast) override;
										// Set SCSI output signal value
	bool WaitSignal(int pin, int ast);
										// Wait for a signal to change
	// Interrupt control
	void DisableIRQ();
										// IRQ Disabled
	void EnableIRQ();
										// IRQ Enabled

	//  GPIO pin functionality settings
	void PinConfig(int pin, int mode);
										// GPIO pin direction setting
	void PullConfig(int pin, int mode);
										// GPIO pin pull up/down resistor setting
	void PinSetSignal(int pin, bool ast);
										// Set GPIO output signal
	void DrvConfig(DWORD drive);
										// Set GPIO drive strength


	mode_e actmode = mode_e::TARGET;	// Operation mode

#if !defined(__x86_64__) && !defined(__X86__)
	uint32_t baseaddr = 0;				// Base address
#endif

	int rpitype = 0;					// Type of Raspberry Pi

	volatile uint32_t *gpio = nullptr;	// GPIO register

	volatile uint32_t *pads = nullptr;	// PADS register

#if !defined(__x86_64__) && !defined(__X86__)
	volatile uint32_t *level = nullptr;	// GPIO input level
#endif

	volatile uint32_t *irpctl = nullptr;	// Interrupt control register

	volatile uint32_t irptenb;			// Interrupt enabled state

	volatile uint32_t *qa7regs = nullptr;	// QA7 register

	volatile int tintcore;				// Interupt control target CPU.

	volatile uint32_t tintctl;			// Interupt control

	volatile uint32_t giccpmr;			// GICC priority setting

#if !defined(__x86_64__) && !defined(__X86__)
	volatile uint32_t *gicd = nullptr;	// GIC Interrupt distributor register
#endif

	volatile uint32_t *gicc = nullptr;	// GIC CPU interface register

	array<DWORD, 4> gpfsel;				// GPFSEL0-4 backup values

	uint32_t signals = 0;				// All bus signals

#ifdef USE_SEL_EVENT_ENABLE
	struct gpioevent_request selevreq = {};	// SEL signal event request

	int epfd;							// epoll file descriptor
#endif	// USE_SEL_EVENT_ENABLE

#if SIGNAL_CONTROL_MODE == 0
	DWORD tblDatMsk[3][256];			// Data mask table

	DWORD tblDatSet[3][256];			// Data setting table
#else
	array<DWORD, 256> tblDatMsk;		// Data mask table

	array<DWORD, 256> tblDatSet;		// Table setting table
#endif

	static const array<int, 19> SignalTable;	// signal table
};

