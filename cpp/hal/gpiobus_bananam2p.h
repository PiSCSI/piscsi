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

#include "hal/data_sample_bananam2p.h"
#include "hal/gpiobus.h"
#include "hal/pi_defs/bpi-gpio.h"
#include "hal/sbc_version.h"
#include "shared/log.h"
#include "shared/scsi.h"
#include <map>

//---------------------------------------------------------------------------
//
//	Class definition
//
//---------------------------------------------------------------------------
class GPIOBUS_BananaM2p : public GPIOBUS
{
  public:
    // Basic Functions
    GPIOBUS_BananaM2p()           = default;
    ~GPIOBUS_BananaM2p() override = default;
    // Destructor
    bool Init(mode_e mode = mode_e::TARGET) override;

    void Cleanup() override;
    void Reset() override;

    //---------------------------------------------------------------------------
    //
    //	Bus signal acquisition
    //
    //---------------------------------------------------------------------------
    uint32_t Acquire() override;

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

    bool WaitREQ(bool ast) override
    {
        return WaitSignal(BPI_PIN_REQ, ast);
    }
    bool WaitACK(bool ast) override
    {
        return WaitSignal(BPI_PIN_ACK, ast);
    }

    // TODO: Restore these back to protected
    //   protected:
    // SCSI I/O signal control
    void MakeTable() override;
    // Create work data
    void SetControl(int pin, bool ast) override;
    // Set Control Signal
    void SetMode(int pin, int mode) override;
    // Set SCSI I/O mode
    int GetMode(int pin) override;

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

    // Extract as specific pin field from a raw data capture
    uint32_t GetPinRaw(uint32_t raw_data, uint32_t pin_num) override;

    unique_ptr<DataSample> GetSample(uint64_t timestamp) override
    {
        Acquire();
        return make_unique<DataSample_BananaM2p>(signals, timestamp);
    }

    // Map the physical pin number to the logical GPIO number
    // shared_ptr<Banana_Pi_Gpio_Mapping> phys_to_gpio_map;

    bool SetupSelEvent();

#if !defined(__x86_64__) && !defined(__X86__)
    uint32_t baseaddr = 0; // Base address
#endif

    volatile uint32_t *gpio = nullptr; // GPIO register

    volatile uint32_t *pads = nullptr; // PADS register

    volatile uint32_t *gpio_map;

#if !defined(__x86_64__) && !defined(__X86__)
    volatile uint32_t *level = nullptr; // GPIO input level
#endif

    volatile uint32_t *irpctl = nullptr; // Interrupt control register

    volatile uint32_t irptenb; // Interrupt enabled state

    volatile uint32_t *qa7regs = nullptr; // QA7 register

    volatile int tintcore; // Interupt control target CPU.

    volatile uint32_t tintctl; // Interupt control

    volatile uint32_t giccpmr; // GICC priority setting

    // Timer control register
    volatile uint32_t *tmr_ctrl;

#if !defined(__x86_64__) && !defined(__X86__)
    volatile uint32_t *gicd = nullptr; // GIC Interrupt distributor register
#endif

    volatile uint32_t *gicc = nullptr; // GIC CPU interface register

    array<uint32_t, 4> gpfsel; // GPFSEL0-4 backup values

    array<uint32_t, 12> signals = {0}; // All bus signals


#if SIGNAL_CONTROL_MODE == 0
    array<array<uint32_t, 256>, 3> tblDatMsk; // Data mask table

    array<array<uint32_t, 256>, 3> tblDatSet; // Data setting table
#else
    array<uint32_t, 256> tblDatMsk = {}; // Data mask table

    array<uint32_t, 256> tblDatSet = {}; // Table setting table
#endif

    int sunxi_setup(void);

    void sunxi_set_pullupdn(int gpio, int pud);
    void sunxi_setup_gpio(int gpio, int direction, int pud);

    void sunxi_output_gpio(int gpio, int value);
    int sunxi_input_gpio(int gpio) const;

    int bpi_found = -1;

    enum class gpio_configure_values_e : uint8_t {
        gpio_input      = 0b000,
        gpio_output     = 0b001,
        gpio_alt_func_1 = 0b010,
        gpio_alt_func_2 = 0b011,
        gpio_reserved_1 = 0b100,
        gpio_reserved_2 = 0b101,
        gpio_interupt   = 0b110,
        gpio_disable    = 0b111
    };

    struct BPIBoards {
        const char *name;
        int gpioLayout;
        int model;
        int rev;
        int mem;
        int maker;
        int warranty;
        int *pinToGpio;
        int *physToGpio;
        int *pinTobcm;
    };

    typedef BPIBoards BpiBoardsType;
    static BpiBoardsType bpiboard[];

    typedef struct sunxi_gpio {
        unsigned int CFG[4];
        unsigned int DAT;
        unsigned int DRV[2];
        unsigned int PULL[2];
    } sunxi_gpio_t;

    /* gpio interrupt control */
    typedef struct sunxi_gpio_int {
        unsigned int CFG[3];
        unsigned int CTL;
        unsigned int STA;
        unsigned int DEB;
    } sunxi_gpio_int_t;

    typedef struct sunxi_gpio_reg {
        struct sunxi_gpio gpio_bank[9];
        unsigned char res[0xbc];
        struct sunxi_gpio_int gpio_int;
    } sunxi_gpio_reg_t;

    volatile uint32_t *pio_map;
    volatile uint32_t *r_pio_map;

    volatile uint32_t *r_gpio_map;

    uint8_t *gpio_mmap_reg;
    uint32_t sunxi_capture_all_gpio();
    void set_pullupdn(int gpio, int pud);

    // These definitions are from c_gpio.c and should be removed at some point!!
    const int SETUP_OK           = 0;

    void short_wait(void) const;

    SBC_Version::sbc_version_type sbc_version;

    void SaveGpioConfig();
    void SaveGpioBankCfg(int);

    sunxi_gpio_reg_t saved_gpio_config;

    static const array<int, 19> SignalTable;

    void InitializeGpio();

};
