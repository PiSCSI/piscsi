//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	[ GPIO-SCSI bus ]
//
//---------------------------------------------------------------------------

#include "hal/gpiobus_allwinner.h"
#include "hal/gpiobus.h"
#include "log.h"

extern int wiringPiMode;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"

bool GPIOBUS_Allwinner::Init(mode_e mode)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
    return false;
}

void GPIOBUS_Allwinner::Cleanup()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__)
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::Reset(){LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)}

BYTE GPIOBUS_Allwinner::GetDAT()
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
    return 0;
}

void GPIOBUS_Allwinner::SetDAT(BYTE dat)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::MakeTable(void)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::SetControl(int pin, bool ast)
{
    GPIOBUS_Allwinner::SetSignal(pin, ast);
}

void GPIOBUS_Allwinner::SetMode(int pin, int mode)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

bool GPIOBUS_Allwinner::GetSignal(int pin) const
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
    return false;
    // return (digitalRead(pin) != 0);
}

void GPIOBUS_Allwinner::SetSignal(int pin, bool ast)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

bool GPIOBUS_Allwinner::WaitSignal(int pin, int ast)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
    return false;
}

void GPIOBUS_Allwinner::DisableIRQ()
{
    LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::EnableIRQ()
{
    LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::PinConfig(int pin, int mode)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::PullConfig(int pin, int mode)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::PinSetSignal(int pin, bool ast)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::DrvConfig(DWORD drive)
{
    (void)drive;
    LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__)
}

uint32_t GPIOBUS_Allwinner::Acquire()
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
    // Only used for development/debugging purposes. Isn't really applicable
    // to any real-world RaSCSI application
    return 0;
}

#pragma GCC diagnostic pop
#pragma GCC diagnostic pop
