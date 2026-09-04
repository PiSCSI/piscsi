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

#include "hal/bus.h"
#include "shared/scsi.h"
#include <memory>
#include <vector>

#ifdef __linux__
#include <linux/gpio.h>
#endif

#include "hal/connection_profile.h"

// #define ENABLE_GPIO_TRACE
#ifdef ENABLE_GPIO_TRACE
#define GPIO_FUNCTION_TRACE LOGTRACE("%s", __PRETTY_FUNCTION__)
#else
#define GPIO_FUNCTION_TRACE
#endif

using namespace std;

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

//---------------------------------------------------------------------------
//
//	Constant declarations (GPIO)
//
//---------------------------------------------------------------------------

const static int GPIO_INPUT    = 0;
const static int GPIO_OUTPUT   = 1;
const static int GPIO_IRQ_IN   = 3;
const static int GPIO_PULLNONE = 0;
const static int GPIO_PULLDOWN = 1;
const static int GPIO_PULLUP   = 2;

//---------------------------------------------------------------------------
//
//	Constant declarations (Control signals)
//
//---------------------------------------------------------------------------
#define ACT_OFF !ACT_ON
#define ENB_OFF !ENB_ON
#define TAD_OUT !TAD_IN
#define IND_OUT !IND_IN
#define DTD_OUT !DTD_IN

//---------------------------------------------------------------------------
//
//	Constant declarations (SCSI)
//
//---------------------------------------------------------------------------
#define IN GPIO_INPUT
#define OUT GPIO_OUTPUT
const static int ON  = 1;
const static int OFF = 0;

//---------------------------------------------------------------------------
//
//	Class definition
//
//---------------------------------------------------------------------------
class GPIOBUS : public BUS
{
  public:
    // Basic Functions
    GPIOBUS() = default;
    // Destructor
    ~GPIOBUS() override = default;
    // Initialization
    bool Init(mode_e mode = mode_e::TARGET) override;

    // Command receive handshake
    int CommandHandShake(vector<uint8_t>&, bool) override;
    // Data receive handshake
    int ReceiveHandShake(uint8_t *, int) override;
    // Data transmission handshake
    int SendHandShake(uint8_t *, int, int) override;

    // SEL signal event polling
    bool PollSelectEvent() override;

  protected:
    virtual void MakeTable() = 0;

    bool GetSignal(int pin) const override     = 0;
    void SetSignal(int pin, bool ast) override = 0;
    bool WaitSignal(int pin, bool ast);

    // Wait for a signal to change
    virtual bool WaitREQ(bool ast) = 0;
    virtual bool WaitACK(bool ast) = 0;

    // Interrupt control
    virtual void EnableIRQ()  = 0;
    virtual void DisableIRQ() = 0;

    // Set GPIO output signal
    virtual void PinSetSignal(int pin, bool ast) = 0;
    // Set GPIO drive strength
    virtual void DrvConfig(uint32_t drive) = 0;

    // Operation mode
    mode_e actmode = mode_e::TARGET;

#ifdef __linux__
    // SEL signal event request
    struct gpioevent_request selevreq = {};
    // epoll file descriptor
    int epfd = 0;

#endif
};
