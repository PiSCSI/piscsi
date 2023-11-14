//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/data_sample_raspberry.h"
#include "hal/gpiobus.h"
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
    void PinConfig(int pin, int mode) override
    {
        (void)pin;
        (void)mode;
    }
    // GPIO pin direction setting
    void PullConfig(int pin, int mode) override
    {
        (void)mode; // Put these in opposite order so Sonar doesn't complain
        (void)pin; // That PinConfig and PullConfig are identical
    }
    // GPIO pin pull up/down resistor setting
    void PinSetSignal(int pin, bool ast) override;
    // Set GPIO output signal
    void DrvConfig(uint32_t drive) override;
    // Set GPIO drive strength

    array<int, 19> SignalTable;
    shared_ptr<uint32_t> signals; // All bus signals

    unique_ptr<DataSample> GetSample(uint64_t timestamp) override
    {
        return make_unique<DataSample_Raspberry>(*signals, timestamp);
    }

#ifdef SHARED_MEMORY_GPIO
    inline static const string SHARED_MEM_MUTEX_NAME = "/sem-mutex";
    inline static const string SHARED_MEM_NAME       = "/posix-shared-mem-example";

    sem_t *mutex_sem, *buffer_count_sem, *spool_signal_sem;
    int fd_shm, fd_log;
#endif
};
