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

#include "hal/board_type.h"
#include "hal/gpiobus.h"
#include "shared/log.h"
#include "shared/scsi.h"
#include <map>
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
    GPIOBUS_Virtual()           = default;
    ~GPIOBUS_Virtual() override = default;
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

    bool GetDP() const override;

    bool WaitREQ(bool ast) override
    {
        return WaitSignal(PIN_REQ, ast);
    }
    bool WaitACK(bool ast) override
    {
        return WaitSignal(PIN_ACK, ast);
    }

    uint8_t GetDAT() override;
    // Get DAT signal
    void SetDAT(uint8_t dat) override;
    // Set DAT signal
  private:
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

    // Get the phase based on raw data
    // TODO: should decode the actual bus phase....
    BUS::phase_t GetPhaseRaw(uint32_t raw_data) override
    {
        (void)raw_data;
        return BUS::phase_t::busfree;
    }

    array<int, 19> SignalTable;
    uint32_t *signals; // All bus signals

#ifdef SHARED_MEMORY_GPIO
    inline static const string SHARED_MEM_MUTEX_NAME = "/sem-mutex";
    inline static const string SHARED_MEM_NAME       = "/posix-shared-mem-example";

    sem_t *mutex_sem, *buffer_count_sem, *spool_signal_sem;
    int fd_shm, fd_log;
#endif
};