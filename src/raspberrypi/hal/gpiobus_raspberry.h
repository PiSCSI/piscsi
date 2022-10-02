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

#include "config.h"
#include "scsi.h"
#include "hal/gpiobus.h"
#include "log.h"

//---------------------------------------------------------------------------
//
//	Class definition
//
//---------------------------------------------------------------------------
class GPIOBUS_Raspberry final : public GPIOBUS
{
public:
	// Basic Functions
	GPIOBUS_Raspberry() = default;
	~GPIOBUS_Raspberry() = default;
	// Destructor
	bool Init(mode_e mode = mode_e::TARGET) override;
										// Initialization
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

	BYTE GetDAT() override;
										// Get DAT signal
	void SetDAT(BYTE dat) override;
										// Set DAT signal
protected:
	// SCSI I/O signal control
	void MakeTable();
										// Create work data
	void SetControl(int pin, bool ast);
										// Set Control Signal
	void SetMode(int pin, int mode);
										// Set SCSI I/O mode
	bool GetSignal(int pin) const override;
										// Get SCSI input signal value
	void SetSignal(int pin, bool ast) override;
										// Set SCSI output signal value
	bool WaitSignal(int pin, int ast);
										// Wait for a signal to change
	// Interrupt control
	void DisableIRQ();
										// IRQ Disabled
	void EnableIRQ();
										// IRQ Enabled

	//  GPIO pin functionality settings
	void PinConfig(int pin, int mode);
										// GPIO pin direction setting
	void PullConfig(int pin, int mode);
										// GPIO pin pull up/down resistor setting
	void PinSetSignal(int pin, bool ast);
										// Set GPIO output signal
	void DrvConfig(DWORD drive);
										// Set GPIO drive strength


#if !defined(__x86_64__) && !defined(__X86__)
	uint32_t baseaddr = 0;				// Base address
#endif

	int rpitype = 0;					// Type of Raspberry Pi

	volatile uint32_t *gpio = nullptr;	// GPIO register

	volatile uint32_t *pads = nullptr;	// PADS register

#if !defined(__x86_64__) && !defined(__X86__)
	volatile uint32_t *level = nullptr;	// GPIO input level
#endif

	volatile uint32_t *irpctl = nullptr;	// Interrupt control register

	volatile uint32_t irptenb;			// Interrupt enabled state

	volatile uint32_t *qa7regs = nullptr;	// QA7 register

	volatile int tintcore;				// Interupt control target CPU.

	volatile uint32_t tintctl;			// Interupt control

	volatile uint32_t giccpmr;			// GICC priority setting

#if !defined(__x86_64__) && !defined(__X86__)
	volatile uint32_t *gicd = nullptr;	// GIC Interrupt distributor register
#endif

	volatile uint32_t *gicc = nullptr;	// GIC CPU interface register

	array<uint32_t, 4> gpfsel;			// GPFSEL0-4 backup values

	uint32_t signals = 0;				// All bus signals

#ifdef USE_SEL_EVENT_ENABLE
	struct gpioevent_request selevreq = {};	// SEL signal event request

	int epfd;							// epoll file descriptor
#endif	// USE_SEL_EVENT_ENABLE

#if SIGNAL_CONTROL_MODE == 0
	array<array<uint32_t, 256>, 3>  tblDatMsk;	// Data mask table

	array<array<uint32_t, 256>, 3> tblDatSet;	// Data setting table
#else
	array<uint32_t, 256> tblDatMsk = {};	// Data mask table

	array<uint32_t, 256> tblDatSet = {};	// Table setting table
#endif

	static const array<int, 19> SignalTable;	// signal table
};
