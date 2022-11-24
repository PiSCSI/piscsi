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

#include "hal/gpiobus.h"
#include "hal/data_sample_raspberry.h"
#include "shared/log.h"
#include "shared/scsi.h"
#include <map>

//---------------------------------------------------------------------------
//
//	SCSI signal pin assignment setting
//	  GPIO pin mapping table for SCSI signals.
//	  PIN_DT0～PIN_SEL
//
//---------------------------------------------------------------------------

#define ALL_SCSI_PINS                                                                                      \
    ((1 << PIN_DT0) | (1 << PIN_DT1) | (1 << PIN_DT2) | (1 << PIN_DT3) | (1 << PIN_DT4) | (1 << PIN_DT5) | \
     (1 << PIN_DT6) | (1 << PIN_DT7) | (1 << PIN_DP) | (1 << PIN_ATN) | (1 << PIN_RST) | (1 << PIN_ACK) |  \
     (1 << PIN_REQ) | (1 << PIN_MSG) | (1 << PIN_CD) | (1 << PIN_IO) | (1 << PIN_BSY) | (1 << PIN_SEL))

#define GPIO_INEDGE ((1 << PIN_BSY) | (1 << PIN_SEL) | (1 << PIN_ATN) | (1 << PIN_ACK) | (1 << PIN_RST))

#define GPIO_MCI ((1 << PIN_MSG) | (1 << PIN_CD) | (1 << PIN_IO))

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
class GPIOBUS_Raspberry : public GPIOBUS
{
  public:
    GPIOBUS_Raspberry()           = default;
    ~GPIOBUS_Raspberry() override = default;
    bool Init(mode_e mode = mode_e::TARGET) override;

    void Reset() override;
    void Cleanup() override;

    //	Bus signal acquisition
    uint32_t Acquire() override;

    // Extract as specific pin field from a raw data capture
    uint32_t GetPinRaw(uint32_t raw_data, uint32_t pin_num) override;

    // Set ENB signal
    void SetENB(bool ast) override;

    // Get BSY signal
    bool GetBSY() const override;
    // Set BSY signal
    void SetBSY(bool ast) override;

    // Get SEL signal
    bool GetSEL() const override;
    // Set SEL signal
    void SetSEL(bool ast) override;

    // Get ATN signal
    bool GetATN() const override;
    // Set ATN signal
    void SetATN(bool ast) override;

    // Get ACK signal
    bool GetACK() const override;
    // Set ACK signal
    void SetACK(bool ast) override;

    // Get ACT signal
    bool GetACT() const override;
    // Set ACT signal
    void SetACT(bool ast) override;

    // Get RST signal
    bool GetRST() const override;
    // Set RST signal
    void SetRST(bool ast) override;

    // Get MSG signal
    bool GetMSG() const override;
    // Set MSG signal
    void SetMSG(bool ast) override;

    // Get CD signal
    bool GetCD() const override;
    // Set CD signal
    void SetCD(bool ast) override;

    // Get IO signal
    bool GetIO() override;
    // Set IO signal
    void SetIO(bool ast) override;

    // Get REQ signal
    bool GetREQ() const override;
    // Set REQ signal
    void SetREQ(bool ast) override;

    bool GetDP() const override;

    // Get DAT signal
    uint8_t GetDAT() override;
    // Set DAT signal
    void SetDAT(uint8_t dat) override;

    bool WaitREQ(bool ast) override
    {
        return WaitSignal(PIN_REQ, ast);
    }
    bool WaitACK(bool ast) override
    {
        return WaitSignal(PIN_ACK, ast);
    }
    static uint32_t bcm_host_get_peripheral_address();

    unique_ptr<DataSample> GetSample(uint64_t timestamp) override
    {
        Acquire();
        return make_unique<DataSample_Raspberry>(signals, timestamp);
    }

  protected:
    // All bus signals
    uint32_t signals = 0;
    // GPIO input level
    volatile uint32_t *level = nullptr;

  private:
    // SCSI I/O signal control
    void MakeTable() override;
    // Create work data
    void SetControl(int pin, bool ast) override;
    // Set Control Signal
    void SetMode(int pin, int mode) override;
    // Set SCSI I/O mode
    int GetMode(int pin) override
    {
        // Not implemented (or needed for thist gpio bus type)
        (void)pin;
        return -1;
    }
    bool GetSignal(int pin) const override;
    // Get SCSI input signal value
    void SetSignal(int pin, bool ast) override;
    // Set SCSI output signal value

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

    static uint32_t get_dt_ranges(const char *filename, uint32_t offset);

    uint32_t baseaddr = 0; // Base address

    int rpitype = 0; // Type of Raspberry Pi

    // GPIO register
    volatile uint32_t *gpio = nullptr; // NOSONAR: volatile needed for register access
    // PADS register
    volatile uint32_t *pads = nullptr; // NOSONAR: volatile needed for register access

    // Interrupt control register
    volatile uint32_t *irpctl = nullptr;

    // Interrupt enabled state
    volatile uint32_t irptenb; // NOSONAR: volatile needed for register access

    // QA7 register
    volatile uint32_t *qa7regs = nullptr;
    // Interupt control target CPU.
    volatile int tintcore; // NOSONAR: volatile needed for register access

    // Interupt control
    volatile uint32_t tintctl; // NOSONAR: volatile needed for register access
    // GICC priority setting
    volatile uint32_t giccpmr; // NOSONAR: volatile needed for register access

#if !defined(__x86_64__) && !defined(__X86__)
    // GIC Interrupt distributor register
    volatile uint32_t *gicd = nullptr;
#endif
    // GIC CPU interface register
    volatile uint32_t *gicc = nullptr;

    // RAM copy of GPFSEL0-4  values (GPIO Function Select)
    array<uint32_t, 4> gpfsel;

#if SIGNAL_CONTROL_MODE == 0
    // Data mask table
    array<array<uint32_t, 256>, 3> tblDatMsk;
    // Data setting table
    array<array<uint32_t, 256>, 3> tblDatSet;
#else
    // Data mask table
    array<uint32_t, 256> tblDatMsk = {};
    // Table setting table
    array<uint32_t, 256> tblDatSet = {};
#endif

    static const array<int, 19> SignalTable;
};
