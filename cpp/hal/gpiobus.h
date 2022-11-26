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

#include "hal/bus.h"
#include "shared/scsi.h"
#include <array>
#include <memory>
#include <vector>

#ifdef __linux__
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
#include "hal/connection_type/connection_standard.h"
#elif defined CONNECT_TYPE_FULLSPEC
#include "hal/connection_type/connection_fullspec.h"
#elif defined CONNECT_TYPE_AIBOM
#include "hal/connection_type/connection_aibom.h"
#elif defined CONNECT_TYPE_GAMERNIUM
#include "hal/connection_type/connection_gamernium.h"
#else
#error Invalid connection type or none specified
#endif

// #define ENABLE_GPIO_TRACE
#ifdef ENABLE_GPIO_TRACE
#define GPIO_FUNCTION_TRACE LOGTRACE("%s", __PRETTY_FUNCTION__)
#else
#define GPIO_FUNCTION_TRACE
#endif

using namespace std;

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

//---------------------------------------------------------------------------
//
//	Constant declarations (GPIO)
//
//---------------------------------------------------------------------------
const static uint32_t SYST_OFFSET = 0x00003000;
const static uint32_t IRPT_OFFSET = 0x0000B200;
const static uint32_t ARMT_OFFSET = 0x0000B400;
const static uint32_t PADS_OFFSET = 0x00100000;
const static uint32_t GPIO_OFFSET = 0x00200000;
const static uint32_t QA7_OFFSET  = 0x01000000;

const static int GPIO_INPUT      = 0;
const static int GPIO_OUTPUT     = 1;
const static int GPIO_IRQ_IN     = 3;
const static int GPIO_PULLNONE   = 0;
const static int GPIO_PULLDOWN   = 1;
const static int GPIO_PULLUP     = 2;
const static int GPIO_FSEL_0     = 0;
const static int GPIO_FSEL_1     = 1;
const static int GPIO_FSEL_2     = 2;
const static int GPIO_FSEL_3     = 3;
const static int GPIO_SET_0      = 7;
const static int GPIO_CLR_0      = 10;
const static int GPIO_LEV_0      = 13;
const static int GPIO_EDS_0      = 16;
const static int GPIO_REN_0      = 19;
const static int GPIO_FEN_0      = 22;
const static int GPIO_HEN_0      = 25;
const static int GPIO_LEN_0      = 28;
const static int GPIO_AREN_0     = 31;
const static int GPIO_AFEN_0     = 34;
const static int GPIO_PUD        = 37;
const static int GPIO_CLK_0      = 38;
const static int GPIO_GPPINMUXSD = 52;
const static int GPIO_PUPPDN0    = 57;
const static int GPIO_PUPPDN1    = 58;
const static int GPIO_PUPPDN3    = 59;
const static int GPIO_PUPPDN4    = 60;
const static int PAD_0_27        = 11;
const static int SYST_CS         = 0;
const static int SYST_CLO        = 1;
const static int SYST_CHI        = 2;
const static int SYST_C0         = 3;
const static int SYST_C1         = 4;
const static int SYST_C2         = 5;
const static int SYST_C3         = 6;
const static int ARMT_LOAD       = 0;
const static int ARMT_VALUE      = 1;
const static int ARMT_CTRL       = 2;
const static int ARMT_CLRIRQ     = 3;
const static int ARMT_RAWIRQ     = 4;
const static int ARMT_MSKIRQ     = 5;
const static int ARMT_RELOAD     = 6;
const static int ARMT_PREDIV     = 7;
const static int ARMT_FREERUN    = 8;
const static int IRPT_PND_IRQ_B  = 0;
const static int IRPT_PND_IRQ_1  = 1;
const static int IRPT_PND_IRQ_2  = 2;
const static int IRPT_FIQ_CNTL   = 3;
const static int IRPT_ENB_IRQ_1  = 4;
const static int IRPT_ENB_IRQ_2  = 5;
const static int IRPT_ENB_IRQ_B  = 6;
const static int IRPT_DIS_IRQ_1  = 7;
const static int IRPT_DIS_IRQ_2  = 8;
const static int IRPT_DIS_IRQ_B  = 9;
const static int QA7_CORE0_TINTC = 16;
const static int GPIO_IRQ        = (32 + 20); // GPIO3

//---------------------------------------------------------------------------
//
//	Constant declarations (Control signals)
//
//---------------------------------------------------------------------------
#define ACT_OFF !ACT_ON
#define ENB_OFF !ENB_ON
#define TAD_OUT !TAD_IN
#define IND_OUT !IND_IN
#define DTD_OUT !DTD_IN

//---------------------------------------------------------------------------
//
//	Constant declarations (SCSI)
//
//---------------------------------------------------------------------------
#define IN GPIO_INPUT
#define OUT GPIO_OUTPUT
const static int ON  = 1;
const static int OFF = 0;

//---------------------------------------------------------------------------
//
//	Constant declarations (bus control timing)
//
//---------------------------------------------------------------------------
// SCSI Bus timings taken from:
//     https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-05.html
const static int SCSI_DELAY_ARBITRATION_DELAY_NS         = 2400;
const static int SCSI_DELAY_ASSERTION_PERIOD_NS          = 90;
const static int SCSI_DELAY_BUS_CLEAR_DELAY_NS           = 800;
const static int SCSI_DELAY_BUS_FREE_DELAY_NS            = 800;
const static int SCSI_DELAY_BUS_SET_DELAY_NS             = 1800;
const static int SCSI_DELAY_BUS_SETTLE_DELAY_NS          = 400;
const static int SCSI_DELAY_CABLE_SKEW_DELAY_NS          = 10;
const static int SCSI_DELAY_DATA_RELEASE_DELAY_NS        = 400;
const static int SCSI_DELAY_DESKEW_DELAY_NS              = 45;
const static int SCSI_DELAY_DISCONNECTION_DELAY_US       = 200;
const static int SCSI_DELAY_HOLD_TIME_NS                 = 45;
const static int SCSI_DELAY_NEGATION_PERIOD_NS           = 90;
const static int SCSI_DELAY_POWER_ON_TO_SELECTION_TIME_S = 10;         // (recommended)
const static int SCSI_DELAY_RESET_TO_SELECTION_TIME_US   = 250 * 1000; // (recommended)
const static int SCSI_DELAY_RESET_HOLD_TIME_US           = 25;
const static int SCSI_DELAY_SELECTION_ABORT_TIME_US      = 200;
const static int SCSI_DELAY_SELECTION_TIMEOUT_DELAY_NS   = 250 * 1000; // (recommended)
const static int SCSI_DELAY_FAST_ASSERTION_PERIOD_NS     = 30;
const static int SCSI_DELAY_FAST_CABLE_SKEW_DELAY_NS     = 5;
const static int SCSI_DELAY_FAST_DESKEW_DELAY_NS         = 20;
const static int SCSI_DELAY_FAST_HOLD_TIME_NS            = 10;
const static int SCSI_DELAY_FAST_NEGATION_PERIOD_NS      = 30;

// The DaynaPort SCSI Link do a short delay in the middle of transfering
// a packet. This is the number of uS that will be delayed between the
// header and the actual data.
const static int SCSI_DELAY_SEND_DATA_DAYNAPORT_US = 100;

//---------------------------------------------------------------------------
//
//	Class definition
//
//---------------------------------------------------------------------------
class GPIOBUS : public BUS
{
  public:
    // Basic Functions
    GPIOBUS()           = default;
    ~GPIOBUS() override = default;
    // Destructor
    virtual bool Init(mode_e mode = mode_e::TARGET) override;
    // Initialization
    virtual void Reset() override = 0;
    // Reset
    virtual void Cleanup() override = 0;
    // Cleanup

    virtual uint32_t Acquire() override = 0;

    virtual void SetENB(bool ast) = 0;
    // Set ENB signal

    int CommandHandShake(vector<uint8_t> &) override;
    // Command receive handshake
    int ReceiveHandShake(uint8_t *buf, int count) override;
    // Data receive handshake
    int SendHandShake(uint8_t *buf, int count, int delay_after_bytes) override;
    // Data transmission handshake

#ifdef USE_SEL_EVENT_ENABLE
    // SEL signal interrupt
    bool PollSelectEvent() override;
    // SEL signal event polling
    void ClearSelectEvent() override;
    // Clear SEL signal event
    // Clear SEL signal event
#endif
    // TODO: restore this back to protected
    // protected:
    virtual void MakeTable()                   = 0;
    virtual void SetControl(int pin, bool ast) = 0;
    virtual void SetMode(int pin, int mode)    = 0;
    bool GetSignal(int pin) const override     = 0;
    void SetSignal(int pin, bool ast) override = 0;
    bool WaitSignal(int pin, bool ast);

    virtual bool WaitREQ(bool ast) = 0;
    virtual bool WaitACK(bool ast) = 0;

    // Wait for a signal to change
    // Interrupt control
    virtual void DisableIRQ() = 0;
    // IRQ Disabled
    virtual void EnableIRQ() = 0;
    // IRQ Enabled

    //  GPIO pin functionality settings
    virtual void PinConfig(int pin, int mode) = 0;
    // GPIO pin direction setting
    virtual void PullConfig(int pin, int mode) = 0;
    // GPIO pin pull up/down resistor setting
    virtual void PinSetSignal(int pin, bool ast) = 0;
    // Set GPIO output signal
    virtual void DrvConfig(uint32_t drive) = 0;
    // Set GPIO drive strength

    mode_e actmode = mode_e::TARGET; // Operation mode
#ifdef USE_SEL_EVENT_ENABLE
    struct gpioevent_request selevreq = {}; // SEL signal event request

    int epfd; // epoll file descriptor
#endif        // USE_SEL_EVENT_ENABLE
};
