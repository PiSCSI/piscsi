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

#include <sys/mman.h>

#include "config.h"
#include "hal/gpiobus.h"
#include "hal/gpiobus_allwinner.h"
#include "hal/systimer.h"
#include "log.h"
#include "os.h"
// #include "wiringPi.h"

extern int wiringPiMode;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"

bool GPIOBUS_Allwinner::Init(mode_e mode)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    return false;
    //     GPIOBUS::Init(mode);
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

    // 	return true;

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
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);

    // #if defined(__x86_64__) || defined(__X86__)
    // 	return;
    // #else
    // 	int i;
    // 	int pin;

    // 	// Release SEL signal interrupt
    // #ifdef USE_SEL_EVENT_ENABLE
    // 	close(selevreq.fd);
    // #endif // USE_SEL_EVENT_ENABLE

    // 	// Set control signals
    // 	PinSetSignal(PIN_ENB, FALSE);
    // 	PinSetSignal(PIN_ACT, FALSE);
    // 	PinSetSignal(PIN_TAD, FALSE);
    // 	PinSetSignal(PIN_IND, FALSE);
    // 	PinSetSignal(PIN_DTD, FALSE);
    // 	PinConfig(PIN_ACT, GPIO_INPUT);
    // 	PinConfig(PIN_TAD, GPIO_INPUT);
    // 	PinConfig(PIN_IND, GPIO_INPUT);
    // 	PinConfig(PIN_DTD, GPIO_INPUT);

    // 	// Initialize all signals
    // 	for (i = 0; SignalTable[i] >= 0; i++)
    // 	{
    // 		pin = SignalTable[i];
    // 		PinSetSignal(pin, FALSE);
    // 		PinConfig(pin, GPIO_INPUT);
    // 		PullConfig(pin, GPIO_PULLNONE);
    // 	}

    // 	// Set drive strength back to 8mA
    // 	DrvConfig(3);
    // #endif // ifdef __x86_64__ || __X86__
}

extern int physToGpio_BPI_M2P[64];

// bool GPIOBUS_Allwinner::SetupPollSelectEvent(){

// #ifdef USE_SEL_EVENT_ENABLE
// 	struct epoll_event ev;

// 	// GPIO chip open
// 	LOGTRACE("%s GPIO chip open", __PRETTY_FUNCTION__);
// 	int fd = open("/dev/gpiochip0", 0);
// 	if (fd == -1)
// 	{
// 		LOGERROR("Unable to open /dev/gpiochip0. Is RaSCSI already running?")
// 		return false;
// 	}

// 	// Event request setting
// 	LOGTRACE("%s Event request setting (pin sel: %d)", __PRETTY_FUNCTION__, physToGpio_BPI_M2P[PIN_SEL]);
// 	strcpy(selevreq.consumer_label, "RaSCSI");

// //#define	BPI_M2P_27	GPIO_PA19
// //#define GPIO_PA19	19

// 	selevreq.lineoffset = 19;
// 	selevreq.handleflags = GPIOHANDLE_REQUEST_INPUT;
// #if SIGNAL_CONTROL_MODE < 2
// 	selevreq.eventflags = GPIOEVENT_REQUEST_FALLING_EDGE;
// #else
// 	selevreq.eventflags = GPIOEVENT_REQUEST_RISING_EDGE;
// #endif // SIGNAL_CONTROL_MODE

// 	// Get event request
// 	LOGTRACE("%s Get event request", __PRETTY_FUNCTION__);
// 	if (ioctl(fd, GPIO_GET_LINEEVENT_IOCTL, &selevreq) == -1)
// 	{
// 		LOGERROR("Unable to register event request. Is RaSCSI already running?")
// 		LOGERROR("[%08X] %s", errno, strerror(errno));
// 		close(fd);
// 		return false;
// 	}

// 	// Close GPIO chip file handle
// 	LOGTRACE("%s Close GPIO chip file handle", __PRETTY_FUNCTION__);
// 	close(fd);

// 	// epoll initialization
// 	LOGTRACE("%s epoll initialization", __PRETTY_FUNCTION__);
// 	epfd = epoll_create(1);
// 	if (epfd == -1)
// 	{
// 		LOGERROR("Unable to create the epoll event");
// 		LOGERROR("[%08X] %s", errno, strerror(errno));
// 		return false;
// 	}
// 	memset(&ev, 0, sizeof(ev));
// 	ev.events = EPOLLIN | EPOLLPRI;
// 	ev.data.fd = selevreq.fd;
// 	int result = epoll_ctl(epfd, EPOLL_CTL_ADD, selevreq.fd, &ev);

//     if(result != 0){
//         LOGERROR("Failed to epoll_ctl()");
// 		LOGERROR("[%08X] %s", errno, strerror(errno));
//         return false;
//     }

// #endif
//     return true;
// }

// void GPIOBUS_Allwinner::Cleanup()
// {
//     return;

// }
void GPIOBUS_Allwinner::Reset()
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    // #if defined(__x86_64__) || defined(__X86__)
    //     return;
    // #else
    //     int i;
    //     int j;

    //     // Turn off active signal
    //     SetControl(PIN_ACT, ACT_OFF);

    // 	// Set all signals to off
    // 	for (i = 0;; i++) {
    // 		j = SignalTable[i];
    // 		if (j < 0) {
    // 			break;
    // 		}

    //         SetSignal(j, OFF);
    //     }

    // 	if (actmode == mode_e::TARGET) {
    // 		// Target mode

    //         // Set target signal to input
    //         SetControl(PIN_TAD, TAD_IN);
    //         SetMode(PIN_BSY, IN);
    //         SetMode(PIN_MSG, IN);
    //         SetMode(PIN_CD, IN);
    //         SetMode(PIN_REQ, IN);
    //         SetMode(PIN_IO, IN);

    //         // Set the initiator signal to input
    //         SetControl(PIN_IND, IND_IN);
    //         SetMode(PIN_SEL, IN);
    //         SetMode(PIN_ATN, IN);
    //         SetMode(PIN_ACK, IN);
    //         SetMode(PIN_RST, IN);

    //         // Set data bus signals to input
    //         SetControl(PIN_DTD, DTD_IN);
    //         SetMode(PIN_DT0, IN);
    //         SetMode(PIN_DT1, IN);
    //         SetMode(PIN_DT2, IN);
    //         SetMode(PIN_DT3, IN);
    //         SetMode(PIN_DT4, IN);
    //         SetMode(PIN_DT5, IN);
    //         SetMode(PIN_DT6, IN);
    //         SetMode(PIN_DT7, IN);
    //         SetMode(PIN_DP, IN);
    // 	} else {
    // 		// Initiator mode

    //         // Set target signal to input
    //         SetControl(PIN_TAD, TAD_IN);
    //         SetMode(PIN_BSY, IN);
    //         SetMode(PIN_MSG, IN);
    //         SetMode(PIN_CD, IN);
    //         SetMode(PIN_REQ, IN);
    //         SetMode(PIN_IO, IN);

    //         // Set the initiator signal to output
    //         SetControl(PIN_IND, IND_OUT);
    //         SetMode(PIN_SEL, OUT);
    //         SetMode(PIN_ATN, OUT);
    //         SetMode(PIN_ACK, OUT);
    //         SetMode(PIN_RST, OUT);

    //         // Set the data bus signals to output
    //         SetControl(PIN_DTD, DTD_OUT);
    //         SetMode(PIN_DT0, OUT);
    //         SetMode(PIN_DT1, OUT);
    //         SetMode(PIN_DT2, OUT);
    //         SetMode(PIN_DT3, OUT);
    //         SetMode(PIN_DT4, OUT);
    //         SetMode(PIN_DT5, OUT);
    //         SetMode(PIN_DT6, OUT);
    //         SetMode(PIN_DT7, OUT);
    //         SetMode(PIN_DP, OUT);
    //     }

    //     // Initialize all signals
    //     signals = 0;
    // #endif // ifdef __x86_64__ || __X86__
}

//---------------------------------------------------------------------------
//
// Get data signals
//
//---------------------------------------------------------------------------
BYTE GPIOBUS_Allwinner::GetDAT()
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    return 0;
    // LOGDEBUG("0:%02X 1:%02X 2:%02X 3:%02X 4:%02X 5:%02X 6:%02X 7:%02X P:%02X", GetSignal(PIN_DT0),
    // GetSignal(PIN_DT1),GetSignal(PIN_DT2),GetSignal(PIN_DT3),GetSignal(PIN_DT4),GetSignal(PIN_DT5),GetSignal(PIN_DT6),GetSignal(PIN_DT7),GetSignal(PIN_DP));
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
}

//---------------------------------------------------------------------------
//
//	Set data signals
//
//---------------------------------------------------------------------------
void GPIOBUS_Allwinner::SetDAT(BYTE dat)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    // // TODO: This is crazy inefficient...
    // SetSignal(PIN_DT0, dat & (1 << 0));
    // SetSignal(PIN_DT1, dat & (1 << 1));
    // SetSignal(PIN_DT2, dat & (1 << 2));
    // SetSignal(PIN_DT3, dat & (1 << 3));
    // SetSignal(PIN_DT4, dat & (1 << 4));
    // SetSignal(PIN_DT5, dat & (1 << 5));
    // SetSignal(PIN_DT6, dat & (1 << 6));
    // SetSignal(PIN_DT7, dat & (1 << 7));
}

//---------------------------------------------------------------------------
//
//	Create work table
//
//---------------------------------------------------------------------------
void GPIOBUS_Allwinner::MakeTable(void) {}

//---------------------------------------------------------------------------
//
//	Control signal setting
//
//   TODO: NOT SURE WHY THIS IS DIFFERNT THAN SETSIGNAL
//
//---------------------------------------------------------------------------
void GPIOBUS_Allwinner::SetControl(int pin, bool ast)
{
    GPIOBUS_Allwinner::SetSignal(pin, ast);
}

//---------------------------------------------------------------------------
//
//	Input/output mode setting
//
//---------------------------------------------------------------------------
void GPIOBUS_Allwinner::SetMode(int pin, int mode)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    // if(mode == GPIO_INPUT){
    //     pinMode(pin, INPUT);

    // }else{
    //     pinMode(pin, OUTPUT);
    // }
}

//---------------------------------------------------------------------------
//
//	Get input signal value
//
//---------------------------------------------------------------------------
bool GPIOBUS_Allwinner::GetSignal(int pin) const
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    return false;
    // return (digitalRead(pin) != 0);
}

//---------------------------------------------------------------------------
//
//	Set output signal value
//
//---------------------------------------------------------------------------
void GPIOBUS_Allwinner::SetSignal(int pin, bool ast)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
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
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    return false;
    // // Get current time
    // uint32_t now = SysTimer::instance().GetTimerLow();

    // // Calculate timeout (3000ms)
    // uint32_t timeout = 3000 * 1000;

    // // end immediately if the signal has changed
    // do {
    // 	// Immediately upon receiving a reset
    // 	Acquire();
    // 	if (GetRST()) {
    // 		return false;
    // 	}

    // 	// Check for the signal edge
    //     if (((signals >> pin) ^ ~ast) & 1) {
    // 		return true;
    // 	}
    // } while ((SysTimer::instance().GetTimerLow() - now) < timeout);

    // // We timed out waiting for the signal
    // return false;
}

void GPIOBUS_Allwinner::DisableIRQ()
{
    LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__);
}

void GPIOBUS_Allwinner::EnableIRQ()
{
    LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__);
}

//---------------------------------------------------------------------------
//
//	Pin direction setting (input/output)
//
// TODO: What is the difference between PinConfig and SetMode????
//
//---------------------------------------------------------------------------
void GPIOBUS_Allwinner::PinConfig(int pin, int mode)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    // if(mode == GPIO_INPUT){
    //     pinMode(pin, INPUT);

    // }else{
    //     pinMode(pin, OUTPUT);
    // }
}

//---------------------------------------------------------------------------
//
//	Pin pull-up/pull-down setting
//
//---------------------------------------------------------------------------
void GPIOBUS_Allwinner::PullConfig(int pin, int mode)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
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
}

//---------------------------------------------------------------------------
//
//	Set output pin
//
//---------------------------------------------------------------------------
void GPIOBUS_Allwinner::PinSetSignal(int pin, bool ast)
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
}

//---------------------------------------------------------------------------
//
//	Set the signal drive strength
//
//---------------------------------------------------------------------------
void GPIOBUS_Allwinner::DrvConfig(DWORD drive)
{
    (void)drive;
    LOGERROR("%s not implemented!!", __PRETTY_FUNCTION__);
}

uint32_t GPIOBUS_Allwinner::Acquire()
{
    LOGWARN("%s NOT IMPLEMENTED", __PRETTY_FUNCTION__);
    // Only used for development/debugging purposes. Isn't really applicable
    // to any real-world RaSCSI application
    return 0;
}

#pragma GCC diagnostic pop
#pragma GCC diagnostic pop