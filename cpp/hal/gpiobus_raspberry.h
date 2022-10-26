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
#include "config.h"
#include "hal/gpiobus.h"
#include "hal/board_type.h"
#include "log.h"
#include "scsi.h"

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
    bool Init(mode_e mode = mode_e::TARGET, board_type::rascsi_board_type_e 
                rascsi_type = board_type::rascsi_board_type_e::BOARD_TYPE_FULLSPEC) override;

    // // Initialization
    // void Reset() override;
    // Reset
    void Cleanup() override;
    // Cleanup

    //---------------------------------------------------------------------------
    //
    //	Bus signal acquisition
    //
    //---------------------------------------------------------------------------
    uint32_t Acquire() override;

    BYTE GetDAT() override;
    // Get DAT signal
    void SetDAT(BYTE dat) override;
    // Set DAT signal
  private:
    // SCSI I/O signal control
    void MakeTable() override;
    // Create work data
    void SetControl(board_type::pi_physical_pin_e pin, bool ast) override;
    // Set Control Signal
    void SetMode(board_type::pi_physical_pin_e pin, int mode) override;
    // Set SCSI I/O mode
    bool GetSignal(board_type::pi_physical_pin_e pin) const override;
    // Get SCSI input signal value
    void SetSignal(board_type::pi_physical_pin_e pin, bool ast) override;
    // Set SCSI output signal value
    bool WaitSignal(board_type::pi_physical_pin_e pin, int ast) override;
    // Wait for a signal to change
    // Interrupt control
    void DisableIRQ() override;
    // IRQ Disabled
    void EnableIRQ() override;
    // IRQ Enabled

    //  GPIO pin functionality settings
    void PinConfig(board_type::pi_physical_pin_e pin, int mode) override;
    // GPIO pin direction setting
    void PullConfig(board_type::pi_physical_pin_e pin, int mode) override;
    // GPIO pin pull up/down resistor setting
    void PinSetSignal(board_type::pi_physical_pin_e pin, bool ast) override;
    // Set GPIO output signal
    void DrvConfig(uint32_t drive) override;
    // Set GPIO drive strength

    // Map the physical pin number to the logical GPIO number
    const static std::map<board_type::pi_physical_pin_e, int> phys_to_gpio_map;

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
};
