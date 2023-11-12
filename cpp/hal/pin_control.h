//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//  Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include <cstdint>

#pragma once

// Virtual functions that must be implemented by the derived gpiobus classes
// to control the GPIO pins
class PinControl
{
  public:
    virtual bool GetBSY() const   = 0;
    virtual void SetBSY(bool ast) = 0;

    virtual bool GetSEL() const   = 0;
    virtual void SetSEL(bool ast) = 0;

    virtual bool GetATN() const   = 0;
    virtual void SetATN(bool ast) = 0;

    virtual bool GetACK() const   = 0;
    virtual void SetACK(bool ast) = 0;

    virtual bool GetRST() const   = 0;
    virtual void SetRST(bool ast) = 0;

    virtual bool GetMSG() const   = 0;
    virtual void SetMSG(bool ast) = 0;

    virtual bool GetCD() const   = 0;
    virtual void SetCD(bool ast) = 0;

    virtual bool GetIO()         = 0;
    virtual void SetIO(bool ast) = 0;

    virtual bool GetREQ() const   = 0;
    virtual void SetREQ(bool ast) = 0;

    virtual bool GetACT() const   = 0;
    virtual void SetACT(bool ast) = 0;

    virtual uint8_t GetDAT()         = 0;
    virtual void SetDAT(uint8_t dat) = 0;

    // Set ENB signal
    virtual void SetENB(bool ast) = 0;

    // GPIO pin direction setting
    virtual void PinConfig(int pin, int mode) = 0;
    // GPIO pin pull up/down resistor setting
    virtual void PullConfig(int pin, int mode) = 0;

    virtual void SetControl(int pin, bool ast) = 0;
    virtual void SetMode(int pin, int mode)    = 0;

    PinControl()          = default;
    virtual ~PinControl() = default;
};
