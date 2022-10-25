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
    GPIOBUS::Init(mode);
//     wiringPiSetup();
//     wiringPiMode = WPI_MODE_GPIO;


// 	LOGTRACE("%s Set Drive Strength", __PRETTY_FUNCTION__);
// 	// Set Drive Strength to 16mA
// 	DrvConfig(7);

// 	// Set pull up/pull down
// 	LOGTRACE("%s Set pull up/down....", __PRETTY_FUNCTION__);

// #if SIGNAL_CONTROL_MODE == 0
// 	int pullmode = GPIO_PULLNONE;
// #elif SIGNAL_CONTROL_MODE == 1
// 	int pullmode = GPIO_PULLUP;
// #else
// 	int pullmode = GPIO_PULLDOWN;
// #endif

// 	// Initialize all signals
// 	LOGTRACE("%s Initialize all signals....", __PRETTY_FUNCTION__);

// 	for (int i = 0; SignalTable[i] >= 0; i++) {
// 		int j = SignalTable[i];
// 		PinSetSignal(j, OFF);
// 		PinConfig(j, GPIO_INPUT);
// 		PullConfig(j, pullmode);
// 	}

// 	// Set control signals
// 	LOGTRACE("%s Set control signals....", __PRETTY_FUNCTION__);
// 	PinSetSignal(PIN_ACT, OFF);
// 	PinSetSignal(PIN_TAD, OFF);
// 	PinSetSignal(PIN_IND, OFF);
// 	PinSetSignal(PIN_DTD, OFF);
// 	PinConfig(PIN_ACT, GPIO_OUTPUT);
// 	PinConfig(PIN_TAD, GPIO_OUTPUT);
// 	PinConfig(PIN_IND, GPIO_OUTPUT);
// 	PinConfig(PIN_DTD, GPIO_OUTPUT);

// 	// Set the ENABLE signal
// 	// This is used to show that the application is running
// 	PinSetSignal(PIN_ENB, ENB_OFF);
// 	PinConfig(PIN_ENB, GPIO_OUTPUT);



// 	// Create work table

// 	LOGTRACE("%s MakeTable....", __PRETTY_FUNCTION__);
// 	MakeTable();

// 	// Finally, enable ENABLE
// 	LOGTRACE("%s Finally, enable ENABLE....", __PRETTY_FUNCTION__);
// 	// Show the user that this app is running
// 	SetControl(PIN_ENB, ENB_ON);

//     // if(!SetupPollSelectEvent()){
//     //     LOGERROR("Failed to setup SELECT poll event");
//     //     return false;
//     // }

	return true;



    // SetMode(PIN_IND, OUTPUT);
    // SetMode(PIN_TAD, OUTPUT);
    // SetMode(PIN_DTD, OUTPUT);
    // PullConfig(PIN_IND, GPIO_PULLUP);
    // PullConfig(PIN_IND, GPIO_PULLUP);
    // PullConfig(PIN_IND, GPIO_PULLUP);


    // SetMode(PIN_DT0, OUTPUT);
    // SetMode(PIN_DT1, OUTPUT);
    // SetMode(PIN_DT2, OUTPUT);
    // SetMode(PIN_DT3, OUTPUT);
    // SetMode(PIN_DT4, OUTPUT);
    // SetMode(PIN_DT5, OUTPUT);
    // SetMode(PIN_DT6, OUTPUT);
    // SetMode(PIN_DT7, OUTPUT);
    // SetMode(PIN_DP, OUTPUT);
    // SetMode(PIN_IO, OUTPUT);
    // SetMode(PIN_TAD, OUTPUT);
    // SetMode(PIN_IND, OUTPUT);
    //     PullConfig(PIN_DT0, GPIO_PULLNONE);
    // PullConfig(PIN_DT1, GPIO_PULLNONE);
    // PullConfig(PIN_DT2, GPIO_PULLNONE);
    // PullConfig(PIN_DT3, GPIO_PULLNONE);
    // PullConfig(PIN_DT4, GPIO_PULLNONE);
    // PullConfig(PIN_DT5, GPIO_PULLNONE);
    // PullConfig(PIN_DT6, GPIO_PULLNONE);
    // PullConfig(PIN_DT7, GPIO_PULLNONE);
    // PullConfig(PIN_DP, GPIO_PULLNONE);
    // PullConfig(PIN_IO, GPIO_PULLNONE);
    //     PullConfig(PIN_TAD, GPIO_PULLNONE);
    //     PullConfig(PIN_IND, GPIO_PULLNONE);

}

void GPIOBUS_Allwinner::Cleanup()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
   
#if defined(__x86_64__) || defined(__X86__)
	return;
#else
	int i;
	int pin;

	// Release SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
	close(selevreq.fd);
#endif // USE_SEL_EVENT_ENABLE

	// // Set control signals
	// PinSetSignal(PIN_ENB, FALSE);
	// PinSetSignal(PIN_ACT, FALSE);
	// PinSetSignal(PIN_TAD, FALSE);
	// PinSetSignal(PIN_IND, FALSE);
	// PinSetSignal(PIN_DTD, FALSE);
	// PinConfig(PIN_ACT, GPIO_INPUT);
	// PinConfig(PIN_TAD, GPIO_INPUT);
	// PinConfig(PIN_IND, GPIO_INPUT);
	// PinConfig(PIN_DTD, GPIO_INPUT);

	// // Initialize all signals
	// for (i = 0; SignalTable[i] >= 0; i++)
	// {
	// 	pin = SignalTable[i];
	// 	PinSetSignal(pin, FALSE);
	// 	PinConfig(pin, GPIO_INPUT);
	// 	PullConfig(pin, GPIO_PULLNONE);
	// }

	// Set drive strength back to 8mA
	DrvConfig(3);
#endif // ifdef __x86_64__ || __X86__
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::Reset()
{
#if defined(__x86_64__) || defined(__X86__)
    return;
#else
    int i;
    int j;

    // Turn off active signal
    SetControl(PIN_ACT, ACT_OFF);

	// Set all signals to off
	for (i = 0;; i++) {
		j = SignalTable[i];
		if (j < 0) {
			break;
		}

        SetSignal(j, RASCSI_PIN_OFF);
    }

	if (actmode == mode_e::TARGET) {
		// Target mode

        // Set target signal to input
        SetControl(PIN_TAD, TAD_IN);
        SetMode(PIN_BSY, RASCSI_PIN_IN);
        SetMode(PIN_MSG, RASCSI_PIN_IN);
        SetMode(PIN_CD,  RASCSI_PIN_IN);
        SetMode(PIN_REQ, RASCSI_PIN_IN);
        SetMode(PIN_IO,  RASCSI_PIN_IN);

        // Set the initiator signal to input
        SetControl(PIN_IND, IND_IN);
        SetMode(PIN_SEL, RASCSI_PIN_IN);
        SetMode(PIN_ATN, RASCSI_PIN_IN);
        SetMode(PIN_ACK, RASCSI_PIN_IN);
        SetMode(PIN_RST, RASCSI_PIN_IN);

        // Set data bus signals to input
        SetControl(PIN_DTD, DTD_IN);
        SetMode(PIN_DT0, RASCSI_PIN_IN);
        SetMode(PIN_DT1, RASCSI_PIN_IN);
        SetMode(PIN_DT2, RASCSI_PIN_IN);
        SetMode(PIN_DT3, RASCSI_PIN_IN);
        SetMode(PIN_DT4, RASCSI_PIN_IN);
        SetMode(PIN_DT5, RASCSI_PIN_IN);
        SetMode(PIN_DT6, RASCSI_PIN_IN);
        SetMode(PIN_DT7, RASCSI_PIN_IN);
        SetMode(PIN_DP,  RASCSI_PIN_IN);
	} else {
		// Initiator mode

        // Set target signal to input
        SetControl(PIN_TAD, TAD_IN);
        SetMode(PIN_BSY, RASCSI_PIN_IN);
        SetMode(PIN_MSG, RASCSI_PIN_IN);
        SetMode(PIN_CD,  RASCSI_PIN_IN);
        SetMode(PIN_REQ, RASCSI_PIN_IN);
        SetMode(PIN_IO,  RASCSI_PIN_IN);

        // Set the initiator signal to output
        SetControl(PIN_IND, IND_OUT);
        SetMode(PIN_SEL, RASCSI_PIN_OUT);
        SetMode(PIN_ATN, RASCSI_PIN_OUT);
        SetMode(PIN_ACK, RASCSI_PIN_OUT);
        SetMode(PIN_RST, RASCSI_PIN_OUT);

        // Set the data bus signals to output
        SetControl(PIN_DTD, DTD_OUT);
        SetMode(PIN_DT0, RASCSI_PIN_OUT);
        SetMode(PIN_DT1, RASCSI_PIN_OUT);
        SetMode(PIN_DT2, RASCSI_PIN_OUT);
        SetMode(PIN_DT3, RASCSI_PIN_OUT);
        SetMode(PIN_DT4, RASCSI_PIN_OUT);
        SetMode(PIN_DT5, RASCSI_PIN_OUT);
        SetMode(PIN_DT6, RASCSI_PIN_OUT);
        SetMode(PIN_DT7, RASCSI_PIN_OUT);
        SetMode(PIN_DP,  RASCSI_PIN_OUT);
    }

    // Initialize all signals
    signals = 0;
#endif // ifdef __x86_64__ || __X86__
}

BYTE GPIOBUS_Allwinner::GetDAT()
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
    // LOGDEBUG("0:%02X 1:%02X 2:%02X 3:%02X 4:%02X 5:%02X 6:%02X 7:%02X P:%02X", GetSignal(PIN_DT0), GetSignal(PIN_DT1),GetSignal(PIN_DT2),GetSignal(PIN_DT3),GetSignal(PIN_DT4),GetSignal(PIN_DT5),GetSignal(PIN_DT6),GetSignal(PIN_DT7),GetSignal(PIN_DP));
    // // TODO: This is crazy inefficient...
    // DWORD data =
    //     ((GetSignal(PIN_DT0) ? 0x01: 0x00)<< 0) |
    //     ((GetSignal(PIN_DT1) ? 0x01: 0x00)<< 1) |
    //     ((GetSignal(PIN_DT2) ? 0x01: 0x00)<< 2) |
    //     ((GetSignal(PIN_DT3) ? 0x01: 0x00)<< 3) |
    //     ((GetSignal(PIN_DT4) ? 0x01: 0x00)<< 0) |
    //     ((GetSignal(PIN_DT5) ? 0x01: 0x00)<< 5) |
    //     ((GetSignal(PIN_DT6) ? 0x01: 0x00)<< 6) |
    //     ((GetSignal(PIN_DT7) ? 0x01: 0x00)<< 7);

    // return (BYTE)data;
    return 0;
}

void GPIOBUS_Allwinner::SetDAT(BYTE dat)
{
    // TODO: This is crazy inefficient...
    // SetSignal(PIN_DT0, dat & (1 << 0));
    // SetSignal(PIN_DT1, dat & (1 << 1));
    // SetSignal(PIN_DT2, dat & (1 << 2));
    // SetSignal(PIN_DT3, dat & (1 << 3));
    // SetSignal(PIN_DT4, dat & (1 << 4));
    // SetSignal(PIN_DT5, dat & (1 << 5));
    // SetSignal(PIN_DT6, dat & (1 << 6));
    // SetSignal(PIN_DT7, dat & (1 << 7));
        LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)

}

void GPIOBUS_Allwinner::MakeTable(void)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
}

void GPIOBUS_Allwinner::SetControl(int pin, bool ast)
{
    // GPIOBUS_Allwinner::SetSignal(pin, ast);
        LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)

}

void GPIOBUS_Allwinner::SetMode(int pin, int mode)
{
    // LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__)
    // if(mode == GPIO_INPUT){
    //     pinMode(pin, INPUT);

    // }else{
    //     pinMode(pin, OUTPUT);
    // }
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
    // digitalWrite(pin, ast);
}


//---------------------------------------------------------------------------
//
//	Wait for signal change
//
// TODO: maybe this should be moved to SCSI_Bus?
//---------------------------------------------------------------------------
bool GPIOBUS_Allwinner::WaitSignal(int pin, int ast)
{
        LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__)

// {
// 	// Get current time
// 	uint32_t now = SysTimer::instance().GetTimerLow();

// 	// Calculate timeout (3000ms)
// 	uint32_t timeout = 3000 * 1000;


// 	// end immediately if the signal has changed
// 	do {
// 		// Immediately upon receiving a reset
// 		Acquire();
// 		if (GetRST()) {
// 			return false;
// 		}

// 		// Check for the signal edge
//         if (((signals >> pin) ^ ~ast) & 1) {
// 			return true;
// 		}
// 	} while ((SysTimer::instance().GetTimerLow() - now) < timeout);

	// We timed out waiting for the signal
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
        LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__)

    // if(mode == GPIO_INPUT){
    //     pinMode(pin, INPUT);

    // }else{
    //     pinMode(pin, OUTPUT);
    // }
}


void GPIOBUS_Allwinner::PullConfig(int pin, int mode)
{

        // switch (mode)
		// {
		// case GPIO_PULLNONE:
        //     pullUpDnControl(pin,PUD_OFF);
		// 	break;
		// case GPIO_PULLUP:
        //     pullUpDnControl(pin,PUD_UP);
		// 		break;
		// case GPIO_PULLDOWN:
		// 	pullUpDnControl(pin,PUD_DOWN);
		// 	break;
		// default:
        //     LOGERROR("%s INVALID PIN MODE", __PRETTY_FUNCTION__);
		// 	return;
		// }
    LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__)
}


void GPIOBUS_Allwinner::PinSetSignal(int pin, bool ast)
{
    // digitalWrite(pin, ast);
    LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__)
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
