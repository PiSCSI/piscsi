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
#include <semaphore.h>

//---------------------------------------------------------------------------
//
//	Class definition
//
//---------------------------------------------------------------------------
class GPIOBUS_Virtual final : public GPIOBUS
{
  public:
    // Basic Functions
    GPIOBUS_Virtual()  = default;
    ~GPIOBUS_Virtual() override = default;
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

    uint8_t GetDAT() override;
    // Get DAT signal
    void SetDAT(uint8_t dat) override;
    // Set DAT signal
   private:
    // SCSI I/O signal control
    void MakeTable() override;
    // Create work data
    void SetControl(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) override;
    // Set Control Signal
    void SetMode(board_type::pi_physical_pin_e pin, board_type::gpio_direction_e mode) override;
    // Set SCSI I/O mode
    bool GetSignal(board_type::pi_physical_pin_e pin) const override;
    // Get SCSI input signal value
    void SetSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) override;
    // Set SCSI output signal value
    bool WaitSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) override;
    // Wait for a signal to change
    // Interrupt control
    void DisableIRQ() override;
    // IRQ Disabled
    void EnableIRQ() override;
    // IRQ Enabled

    //  GPIO pin functionality settings
    void PinConfig(board_type::pi_physical_pin_e pin, board_type::gpio_direction_e mode) override;
    // GPIO pin direction setting
    void PullConfig(board_type::pi_physical_pin_e pin, board_type::gpio_pull_up_down_e mode) override;
    // GPIO pin pull up/down resistor setting
    void PinSetSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast) override;
    // Set GPIO output signal
    void DrvConfig(uint32_t drive) override;
    // Set GPIO drive strength

    static const std::map<board_type::pi_physical_pin_e, int> phys_to_gpio_map;

    uint32_t *signals; // All bus signals
#ifdef SHARED_MEMORY_GPIO
    inline static const string SHARED_MEM_MUTEX_NAME = "/sem-mutex";
    inline static const string SHARED_MEM_NAME = "/posix-shared-mem-example";

    sem_t *mutex_sem, *buffer_count_sem, *spool_signal_sem;
    int fd_shm, fd_log;
#endif
};
