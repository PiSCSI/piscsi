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
#include "hal/board_type.h"
#include "scsi.h"
#include "bus.h"
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

// #if defined CONNECT_TYPE_STANDARD
// #include "hal/gpiobus_standard.h"
// #elif defined CONNECT_TYPE_FULLSPEC
// #include "hal/gpiobus_fullspec.h"
// #elif defined CONNECT_TYPE_AIBOM
// #include "hal/gpiobus_aibom.h"
// #elif defined CONNECT_TYPE_GAMERNIUM
// #include "hal/gpiobus_gamernium.h"
// #else
// #error Invalid connection type or none specified
// #endif
#define ENABLE_GPIO_TRACE
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

#define ALL_SCSI_PINS                              \
    ((1 << phys_to_gpio_map.at(board->pin_dt0))) | \
    ((1 << phys_to_gpio_map.at(board->pin_dt1))) | \
    ((1 << phys_to_gpio_map.at(board->pin_dt2))) | \
    ((1 << phys_to_gpio_map.at(board->pin_dt3))) | \
    ((1 << phys_to_gpio_map.at(board->pin_dt4))) | \
    ((1 << phys_to_gpio_map.at(board->pin_dt5))) | \
    ((1 << phys_to_gpio_map.at(board->pin_dt6))) | \
    ((1 << phys_to_gpio_map.at(board->pin_dt7))) | \
    ((1 << phys_to_gpio_map.at(board->pin_dp)))  | \
    ((1 << phys_to_gpio_map.at(board->pin_atn))) | \
    ((1 << phys_to_gpio_map.at(board->pin_rst))) | \
    ((1 << phys_to_gpio_map.at(board->pin_ack))) | \
    ((1 << phys_to_gpio_map.at(board->pin_req))) | \
    ((1 << phys_to_gpio_map.at(board->pin_msg))) | \
    ((1 << phys_to_gpio_map.at(board->pin_cd)))  | \
    ((1 << phys_to_gpio_map.at(board->pin_io)))  | \
    ((1 << phys_to_gpio_map.at(board->pin_bsy))  | \
    ((1 << phys_to_gpio_map.at(board->pin_sel)))

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

// #define GPIO_INEDGE ((1 << PIN_BSY) | (1 << PIN_SEL) | (1 << PIN_ATN) | (1 << PIN_ACK) | (1 << PIN_RST))

// #define GPIO_MCI ((1 << PIN_MSG) | (1 << PIN_CD) | (1 << PIN_IO))

//---------------------------------------------------------------------------
//
//	Constant declarations (GIC)
//
//---------------------------------------------------------------------------
const static uint32_t ARM_GICD_BASE = 0xFF841000;
const static uint32_t ARM_GICC_BASE = 0xFF842000;
const static uint32_t ARM_GIC_END   = 0xFF847FFF;
const static int GICD_CTLR          = 0x000;
const static int GICD_IGROUPR0      = 0x020;
const static int GICD_ISENABLER0    = 0x040;
const static int GICD_ICENABLER0    = 0x060;
const static int GICD_ISPENDR0      = 0x080;
const static int GICD_ICPENDR0      = 0x0A0;
const static int GICD_ISACTIVER0    = 0x0C0;
const static int GICD_ICACTIVER0    = 0x0E0;
const static int GICD_IPRIORITYR0   = 0x100;
const static int GICD_ITARGETSR0    = 0x200;
const static int GICD_ICFGR0        = 0x300;
const static int GICD_SGIR          = 0x3C0;
const static int GICC_CTLR          = 0x000;
const static int GICC_PMR           = 0x001;
const static int GICC_IAR           = 0x003;
const static int GICC_EOIR          = 0x004;

//---------------------------------------------------------------------------
//
//	Constant declarations (GIC IRQ)
//
//---------------------------------------------------------------------------
const static int GIC_IRQLOCAL0 = (16 + 14);
const static int GIC_GPIO_IRQ  = (32 + 116); // GPIO3

//---------------------------------------------------------------------------
//
//	Class definition
//
//---------------------------------------------------------------------------
class GPIOBUS : public BUS
{
  public:
    static GPIOBUS *create();

    // Basic Functions
    GPIOBUS()           = default;
    ~GPIOBUS() override = default;
    // Destructor
    bool Init(mode_e mode = mode_e::TARGET, board_type::rascsi_board_type_e rascsi_type =
                                                board_type::rascsi_board_type_e::BOARD_TYPE_FULLSPEC) override;
    
    void InitializeGpio();
    
    // Initialization
    void Reset() override;
    // Reset
    void Cleanup() override;
    // Cleanup

    uint32_t Acquire() override = 0;

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
    bool GetDP() const override;
    // Get Data parity signal

    // Extract as specific pin field from a raw data capture
    uint32_t GetPinRaw(uint32_t raw_data, board_type::pi_physical_pin_e pin_num);
    // TODO: SCSIMON needs to be re-worked to work differently. For now, a quick
    // and dirty hack is just to expose the current board type data structure.
    shared_ptr<board_type::Rascsi_Board_Type> GetBoard()
    {
        return board;
    }

    int CommandHandShake(vector<uint8_t>&) override;
    // Command receive handshake
    int ReceiveHandShake(uint8_t *buf, int count) override;
    // Data receive handshake
    int SendHandShake(uint8_t *buf, int count, int delay_after_bytes) override;
    // Data transmission handshake

    // Get the phase based on raw data
    static BUS::phase_t GetPhaseRaw(uint32_t raw_data);


    const string GetConnectDesc();

#ifdef USE_SEL_EVENT_ENABLE
    // SEL signal interrupt
    bool PollSelectEvent();
    // SEL signal event polling
    void ClearSelectEvent();
    // Clear SEL signal event
#endif // USE_SEL_EVENT_ENABLE

// TODO: THIS SHOULD BE PROTECTED
    static vector<board_type::pi_physical_pin_e> SignalTable; // signal table

// TODO: RESTORE THIS TO PROTECTED!!!
//   protected:
    // SCSI I/O signal control
    virtual void MakeTable() = 0;
    // Create work data
    virtual void SetControl(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) = 0;
    // Set Control Signal
    virtual void SetMode(board_type::pi_physical_pin_e pin, board_type::gpio_direction_e mode) = 0;
    // Set SCSI I/O mode
    bool GetSignal(board_type::pi_physical_pin_e pin) const override = 0;
    // Set Control Signal
    void SetSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) override = 0;
    // Set SCSI I/O mode
    virtual bool WaitSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) = 0;
    // Wait for a signal to change
    // Interrupt control
    virtual void DisableIRQ() = 0;
    // IRQ Disabled
    virtual void EnableIRQ() = 0;
    // IRQ Enabled

    //  GPIO pin functionality settings
    virtual void PinConfig(board_type::pi_physical_pin_e pin, board_type::gpio_direction_e mode) = 0;
    // GPIO pin direction setting
    virtual void PullConfig(board_type::pi_physical_pin_e pin, board_type::gpio_pull_up_down_e mode) = 0;
    // GPIO pin pull up/down resistor setting
    virtual void PinSetSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) = 0;
    // Set GPIO output signal
    virtual void DrvConfig(uint32_t drive) = 0;
    // Set GPIO drive strength

    mode_e actmode = mode_e::TARGET; // Operation mode

    shared_ptr<board_type::Rascsi_Board_Type> board = nullptr;

#if !defined(__x86_64__) && !defined(__X86__)
    uint32_t baseaddr = 0; // Base address
#endif



#ifdef USE_SEL_EVENT_ENABLE
    struct gpioevent_request selevreq = {}; // SEL signal event request

    int epfd; // epoll file descriptor
#endif        // USE_SEL_EVENT_ENABLE

  private:
    int rpitype = 0; // Type of Raspberry Pi

    volatile uint32_t *gpio = nullptr; // GPIO register

    volatile uint32_t *pads = nullptr; // PADS register

#if !defined(__x86_64__) && !defined(__X86__)
    volatile uint32_t *level = nullptr; // GPIO input level
#endif

    volatile uint32_t *irpctl = nullptr; // Interrupt control register

    volatile uint32_t irptenb; // Interrupt enabled state

    volatile uint32_t *qa7regs = nullptr; // QA7 register

    volatile int tintcore; // Interupt control target CPU.

    volatile uint32_t tintctl; // Interupt control

    volatile uint32_t giccpmr; // GICC priority setting

#if !defined(__x86_64__) && !defined(__X86__)
    volatile uint32_t *gicd = nullptr; // GIC Interrupt distributor register
#endif

    volatile uint32_t *gicc = nullptr; // GIC CPU interface register

    array<uint32_t, 4> gpfsel; // GPFSEL0-4 backup values



#if SIGNAL_CONTROL_MODE == 0
    array<array<uint32_t, 256>, 3> tblDatMsk; // Data mask table

    array<array<uint32_t, 256>, 3> tblDatSet; // Data setting table
#else
    array<uint32_t, 256> tblDatMsk = {}; // Data mask table

    array<uint32_t, 256> tblDatSet = {}; // Table setting table
#endif
};
