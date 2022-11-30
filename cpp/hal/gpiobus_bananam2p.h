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
#include "hal/sunxi_utils.h"
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

    bool GetDP() const override;

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

    // Set SCSI output signal value
    void SetSignal(int pin, bool ast) override;

    // Interrupt control
    // IRQ Disabled
    void DisableIRQ() override;
    // IRQ Enabled
    void EnableIRQ() override;

    //  GPIO pin functionality settings
    void PinConfig(int pin, int mode) override;
    // GPIO pin direction setting
    void PullConfig(int pin, int mode) override;
    // GPIO pin pull up/down resistor setting
    void PinSetSignal(int pin, bool ast) override;
    // Set GPIO output signal
    void DrvConfig(uint32_t drive) override;
    // Set GPIO drive strength

    unique_ptr<DataSample> GetSample(uint64_t timestamp) override
    {
        Acquire();
        return make_unique<DataSample_BananaM2p>(signals, timestamp);
    }

    bool SetupSelEvent();

#if !defined(__x86_64__) && !defined(__X86__)
    uint32_t baseaddr = 0; // Base address
#endif

    volatile uint32_t *gpio_map = nullptr;

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

    volatile SunXI::sunxi_gpio_reg_t *pio_map;
    volatile SunXI::sunxi_gpio_reg_t *r_pio_map;

    volatile uint32_t *r_gpio_map;

    uint8_t *gpio_mmap_reg;
    uint32_t sunxi_capture_all_gpio();
    void set_pullupdn(int gpio, int pud);

    // These definitions are from c_gpio.c and should be removed at some point!!
    const int SETUP_OK = 0;

    SBC_Version::sbc_version_type sbc_version;

    SunXI::sunxi_gpio_reg_t saved_gpio_config;

    static const array<int, 19> SignalTable;

    void InitializeGpio();
    std::vector<int> gpio_banks;

#if defined(__x86_64__) || defined(__X86__)
    // The SEL_EVENT functions need to do something to prevent SonarCloud from
    // claiming they should be const
    int dummy_var = 0;
#endif
};
