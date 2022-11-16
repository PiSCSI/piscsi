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

#include <map>
#include "hal/gpiobus.h"
#include "hal/board_type.h"
#include "shared/log.h"
#include "shared/scsi.h"

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
//	Class definition
//
//---------------------------------------------------------------------------
class GPIOBUS_Raspberry final : public GPIOBUS
{
  public:
    // Basic Functions
    GPIOBUS_Raspberry()  = default;
    ~GPIOBUS_Raspberry() override = default;
    // Destructor
    bool Init(mode_e mode = mode_e::TARGET) override;

    // // Initialization
    void Reset() override;
    // Reset
    void Cleanup() override;
    // Cleanup

    //---------------------------------------------------------------------------
    //
    //	Bus signal acquisition
    //
    //---------------------------------------------------------------------------
    uint32_t Acquire() override;

	// Extract as specific pin field from a raw data capture
	uint32_t GetPinRaw(uint32_t raw_data, uint32_t pin_num) override;


      void SetENB(bool ast) override;
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

      bool GetACT() const override;
    // Get ACT signal
      void SetACT(bool ast) override;
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

     virtual bool GetDP() const override;




    uint8_t GetDAT() override;
    // Get DAT signal
    void SetDAT(uint8_t dat) override;
    // Set DAT signal


    bool WaitREQ(bool ast) override {return WaitSignal(PIN_REQ, ast);}
    bool WaitACK(bool ast) override {return WaitSignal(PIN_ACK, ast);}

    void dump_all() override {}


// TODO: Restore this back to private
//   private:
    // SCSI I/O signal control
    void MakeTable() override;
    // Create work data
    void SetControl(int pin, bool ast) override;
    // Set Control Signal
    void SetMode(int pin, int mode) override;
    // Set SCSI I/O mode
    bool GetSignal(int pin) const override;
    // Get SCSI input signal value
    void SetSignal(int pin, bool ast) override;
    // Set SCSI output signal value
    // bool WaitSignal(int pin, bool ast) override;
    // Wait for a signal to change
    // Interrupt control
    void DisableIRQ() override;
    // IRQ Disabled
    void EnableIRQ() override;
    // IRQ Enabled

    //  GPIO pin functionality settings
    void PinConfig(int pin, int mode) override;
    // GPIO pin direction setting
    void PullConfig(int pin, int mode) override;
    // GPIO pin pull up/down resistor setting
    void PinSetSignal(int pin, bool ast) override;
    // Set GPIO output signal
    void DrvConfig(uint32_t drive) override;
    // Set GPIO drive strength

    // // Map the physical pin number to the logical GPIO number
    // const static std::map<int, int> phys_to_gpio_map;

    BUS::phase_t GetPhaseRaw(uint32_t raw_data) override;

#if !defined(__x86_64__) && !defined(__X86__)
    uint32_t baseaddr = 0; // Base address
#endif

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

    uint32_t signals = 0; // All bus signals

#if SIGNAL_CONTROL_MODE == 0
    array<array<uint32_t, 256>, 3> tblDatMsk; // Data mask table

    array<array<uint32_t, 256>, 3> tblDatSet; // Data setting table
#else
    array<uint32_t, 256> tblDatMsk = {}; // Data mask table

    array<uint32_t, 256> tblDatSet = {}; // Table setting table
#endif

    static const array<int, 19> SignalTable;    
};
