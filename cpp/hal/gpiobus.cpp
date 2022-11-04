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

#include "hal/gpiobus.h"
#include "config.h"
#include "hal/sbc_version.h"
#include "hal/systimer.h"
#include "log.h"
#include <array>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif

#if defined CONNECT_TYPE_STANDARD
#include "hal/gpiobus_standard.h"
#elif defined CONNECT_TYPE_FULLSPEC
#include "hal/gpiobus_fullspec.h"
#elif defined CONNECT_TYPE_AIBOM
#include "hal/gpiobus_aibom.h"
#elif defined CONNECT_TYPE_GAMERNIUM
#include "hal/gpiobus_gamernium.h"
#else
#error Invalid connection type or none specified
#endif

//---------------------------------------------------------------------------
//
//	Constant declarations (bus control timing)
//
//---------------------------------------------------------------------------
// SCSI Bus timings taken from:
//     https://www.staff.uni-mainz.de/tacke/scsi/SCSI2-05.html
[[maybe_unused]] const static int SCSI_DELAY_ARBITRATION_DELAY_NS         = 2400;
[[maybe_unused]] const static int SCSI_DELAY_ASSERTION_PERIOD_NS          = 90;
[[maybe_unused]] const static int SCSI_DELAY_BUS_CLEAR_DELAY_NS           = 800;
[[maybe_unused]] const static int SCSI_DELAY_BUS_FREE_DELAY_NS            = 800;
[[maybe_unused]] const static int SCSI_DELAY_BUS_SET_DELAY_NS             = 1800;
[[maybe_unused]] const static int SCSI_DELAY_BUS_SETTLE_DELAY_NS          = 400;
[[maybe_unused]] const static int SCSI_DELAY_CABLE_SKEW_DELAY_NS          = 10;
[[maybe_unused]] const static int SCSI_DELAY_DATA_RELEASE_DELAY_NS        = 400;
[[maybe_unused]] const static int SCSI_DELAY_DESKEW_DELAY_NS              = 45;
[[maybe_unused]] const static int SCSI_DELAY_DISCONNECTION_DELAY_US       = 200;
[[maybe_unused]] const static int SCSI_DELAY_HOLD_TIME_NS                 = 45;
[[maybe_unused]] const static int SCSI_DELAY_NEGATION_PERIOD_NS           = 90;
[[maybe_unused]] const static int SCSI_DELAY_POWER_ON_TO_SELECTION_TIME_S = 10;         // (recommended)
[[maybe_unused]] const static int SCSI_DELAY_RESET_TO_SELECTION_TIME_US   = 250 * 1000; // (recommended)
[[maybe_unused]] const static int SCSI_DELAY_RESET_HOLD_TIME_US           = 25;
[[maybe_unused]] const static int SCSI_DELAY_SELECTION_ABORT_TIME_US      = 200;
[[maybe_unused]] const static int SCSI_DELAY_SELECTION_TIMEOUT_DELAY_NS   = 250 * 1000; // (recommended)
[[maybe_unused]] const static int SCSI_DELAY_FAST_ASSERTION_PERIOD_NS     = 30;
[[maybe_unused]] const static int SCSI_DELAY_FAST_CABLE_SKEW_DELAY_NS     = 5;
[[maybe_unused]] const static int SCSI_DELAY_FAST_DESKEW_DELAY_NS         = 20;
[[maybe_unused]] const static int SCSI_DELAY_FAST_HOLD_TIME_NS            = 10;
[[maybe_unused]] const static int SCSI_DELAY_FAST_NEGATION_PERIOD_NS      = 30;

// The DaynaPort SCSI Link do a short delay in the middle of transfering
// a packet. This is the number of uS that will be delayed between the
// header and the actual data.
const static int SCSI_DELAY_SEND_DATA_DAYNAPORT_US = 100;

using namespace std;

// Nothing SBC hardware specific should be done in this function
bool GPIOBUS::Init(mode_e mode)
{
    GPIO_FUNCTION_TRACE
    // Save operation mode
    actmode = mode;
    return true;
}

void GPIOBUS::Cleanup()
{
#if defined(__x86_64__) || defined(__X86__)
    return;
#else
    // Release SEL signal interrupt
#ifdef USE_SEL_EVENT_ENABLE
    close(selevreq.fd);
#endif // USE_SEL_EVENT_ENABLE

    // Set control signals
    PinSetSignal(PIN_ENB, OFF);
    PinSetSignal(PIN_ACT, OFF);
    PinSetSignal(PIN_TAD, OFF);
    PinSetSignal(PIN_IND, OFF);
    PinSetSignal(PIN_DTD, OFF);
    PinConfig(PIN_ACT, GPIO_INPUT);
    PinConfig(PIN_TAD, GPIO_INPUT);
    PinConfig(PIN_IND, GPIO_INPUT);
    PinConfig(PIN_DTD, GPIO_INPUT);

    // Initialize all signals
    for (int i = 0; SignalTable[i] >= 0; i++) {
        int pin = SignalTable[i];
        PinSetSignal(pin, OFF);
        PinConfig(pin, GPIO_INPUT);
        PullConfig(pin, GPIO_PULLNONE);
    }

    // Set drive strength back to 8mA
    DrvConfig(3);
#endif // ifdef __x86_64__ || __X86__
}

void GPIOBUS::Reset()
{
    GPIO_FUNCTION_TRACE
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

        SetSignal(j, OFF);
    }

    if (actmode == mode_e::TARGET) {
        // Target mode

        // Set target signal to input
        SetControl(PIN_TAD, TAD_IN);
        SetMode(PIN_BSY, IN);
        SetMode(PIN_MSG, IN);
        SetMode(PIN_CD, IN);
        SetMode(PIN_REQ, IN);
        SetMode(PIN_IO, IN);

        // Set the initiator signal to input
        SetControl(PIN_IND, IND_IN);
        SetMode(PIN_SEL, IN);
        SetMode(PIN_ATN, IN);
        SetMode(PIN_ACK, IN);
        SetMode(PIN_RST, IN);

        // Set data bus signals to input
        SetControl(PIN_DTD, DTD_IN);
        SetMode(PIN_DT0, IN);
        SetMode(PIN_DT1, IN);
        SetMode(PIN_DT2, IN);
        SetMode(PIN_DT3, IN);
        SetMode(PIN_DT4, IN);
        SetMode(PIN_DT5, IN);
        SetMode(PIN_DT6, IN);
        SetMode(PIN_DT7, IN);
        SetMode(PIN_DP, IN);
    } else {
        // Initiator mode

        // Set target signal to input
        SetControl(PIN_TAD, TAD_IN);
        SetMode(PIN_BSY, IN);
        SetMode(PIN_MSG, IN);
        SetMode(PIN_CD, IN);
        SetMode(PIN_REQ, IN);
        SetMode(PIN_IO, IN);

        // Set the initiator signal to output
        SetControl(PIN_IND, IND_OUT);
        SetMode(PIN_SEL, OUT);
        SetMode(PIN_ATN, OUT);
        SetMode(PIN_ACK, OUT);
        SetMode(PIN_RST, OUT);

        // Set the data bus signals to output
        SetControl(PIN_DTD, DTD_OUT);
        SetMode(PIN_DT0, OUT);
        SetMode(PIN_DT1, OUT);
        SetMode(PIN_DT2, OUT);
        SetMode(PIN_DT3, OUT);
        SetMode(PIN_DT4, OUT);
        SetMode(PIN_DT5, OUT);
        SetMode(PIN_DT6, OUT);
        SetMode(PIN_DT7, OUT);
        SetMode(PIN_DP, OUT);
    }

    // Initialize all signals
    signals = 0;
#endif // ifdef __x86_64__ || __X86__
}

void GPIOBUS::SetENB(bool ast)
{
    GPIO_FUNCTION_TRACE
    PinSetSignal(PIN_ENB, ast ? ENB_ON : ENB_OFF);
}

bool GPIOBUS::GetBSY() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_BSY);
}

void GPIOBUS::SetBSY(bool ast)
{
    GPIO_FUNCTION_TRACE
    // Set BSY signal
    SetSignal(PIN_BSY, ast);

    if (actmode == mode_e::TARGET) {
        if (ast) {
            // Turn on ACTIVE signal
            SetControl(PIN_ACT, ACT_ON);

            // Set Target signal to output
            SetControl(PIN_TAD, TAD_OUT);

            SetMode(PIN_BSY, OUT);
            SetMode(PIN_MSG, OUT);
            SetMode(PIN_CD, OUT);
            SetMode(PIN_REQ, OUT);
            SetMode(PIN_IO, OUT);
        } else {
            // Turn off the ACTIVE signal
            SetControl(PIN_ACT, ACT_OFF);

            // Set the target signal to input
            SetControl(PIN_TAD, TAD_IN);

            SetMode(PIN_BSY, IN);
            SetMode(PIN_MSG, IN);
            SetMode(PIN_CD, IN);
            SetMode(PIN_REQ, IN);
            SetMode(PIN_IO, IN);
        }
    }
}

bool GPIOBUS::GetSEL() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_SEL);
}

void GPIOBUS::SetSEL(bool ast)
{
    GPIO_FUNCTION_TRACE
    if (actmode == mode_e::INITIATOR && ast) {
        // Turn on ACTIVE signal
        SetControl(PIN_ACT, ACT_ON);
    }

    // Set SEL signal
    SetSignal(PIN_SEL, ast);
}

bool GPIOBUS::GetATN() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_ATN);
}

void GPIOBUS::SetATN(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(PIN_ATN, ast);
}

bool GPIOBUS::GetACK() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_ACK);
}

void GPIOBUS::SetACK(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(PIN_ACK, ast);
}

bool GPIOBUS::GetACT() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_ACT);
}

void GPIOBUS::SetACT(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(PIN_ACT, ast);
}

bool GPIOBUS::GetRST() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_RST);
}

void GPIOBUS::SetRST(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(PIN_RST, ast);
}

bool GPIOBUS::GetMSG() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_MSG);
}

void GPIOBUS::SetMSG(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(PIN_MSG, ast);
}

bool GPIOBUS::GetCD() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_CD);
}

void GPIOBUS::SetCD(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(PIN_CD, ast);
}

bool GPIOBUS::GetIO()
{
    GPIO_FUNCTION_TRACE
    bool ast = GetSignal(PIN_IO);

    if (actmode == mode_e::INITIATOR) {
        // Change the data input/output direction by IO signal
        if (ast) {
            SetControl(PIN_DTD, DTD_IN);
            SetMode(PIN_DT0, IN);
            SetMode(PIN_DT1, IN);
            SetMode(PIN_DT2, IN);
            SetMode(PIN_DT3, IN);
            SetMode(PIN_DT4, IN);
            SetMode(PIN_DT5, IN);
            SetMode(PIN_DT6, IN);
            SetMode(PIN_DT7, IN);
            SetMode(PIN_DP, IN);
        } else {
            SetControl(PIN_DTD, DTD_OUT);
            SetMode(PIN_DT0, OUT);
            SetMode(PIN_DT1, OUT);
            SetMode(PIN_DT2, OUT);
            SetMode(PIN_DT3, OUT);
            SetMode(PIN_DT4, OUT);
            SetMode(PIN_DT5, OUT);
            SetMode(PIN_DT6, OUT);
            SetMode(PIN_DT7, OUT);
            SetMode(PIN_DP, OUT);
        }
    }

    return ast;
}

void GPIOBUS::SetIO(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(PIN_IO, ast);

    if (actmode == mode_e::TARGET) {
        // Change the data input/output direction by IO signal
        if (ast) {
            SetControl(PIN_DTD, DTD_OUT);
            SetDAT(0);
            SetMode(PIN_DT0, OUT);
            SetMode(PIN_DT1, OUT);
            SetMode(PIN_DT2, OUT);
            SetMode(PIN_DT3, OUT);
            SetMode(PIN_DT4, OUT);
            SetMode(PIN_DT5, OUT);
            SetMode(PIN_DT6, OUT);
            SetMode(PIN_DT7, OUT);
            SetMode(PIN_DP, OUT);
        } else {
            SetControl(PIN_DTD, DTD_IN);
            SetMode(PIN_DT0, IN);
            SetMode(PIN_DT1, IN);
            SetMode(PIN_DT2, IN);
            SetMode(PIN_DT3, IN);
            SetMode(PIN_DT4, IN);
            SetMode(PIN_DT5, IN);
            SetMode(PIN_DT6, IN);
            SetMode(PIN_DT7, IN);
            SetMode(PIN_DP, IN);
        }
    }
}

bool GPIOBUS::GetREQ() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_REQ);
}

void GPIOBUS::SetREQ(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(PIN_REQ, ast);
}

bool GPIOBUS::GetDP() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(PIN_DP);
}

//---------------------------------------------------------------------------
//
//	Receive command handshake
//
//---------------------------------------------------------------------------
int GPIOBUS::CommandHandShake(uint8_t *buf, int command_byte_count)
{
    GPIO_FUNCTION_TRACE
    // Only works in TARGET mode
    if (actmode != mode_e::TARGET) {
        return 0;
    }

    DisableIRQ();

    // Assert REQ signal
    SetSignal(PIN_REQ, ON);

    // Wait for ACK signal
    bool ret = WaitSignal(PIN_ACK, ON);

    // Wait until the signal line stabilizes
    SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

    // Get data
    *buf = GetDAT();

    // Disable REQ signal
    SetSignal(PIN_REQ, OFF);

    // Timeout waiting for ACK assertion
    if (!ret) {
        EnableIRQ();
        return 0;
    }

    // Wait for ACK to clear
    ret = WaitSignal(PIN_ACK, OFF);

    // Timeout waiting for ACK to clear
    if (!ret) {
        EnableIRQ();
        return 0;
    }

    // The ICD AdSCSI ST, AdSCSI Plus ST and AdSCSI Micro ST host adapters allow SCSI devices to be connected
    // to the ACSI bus of Atari ST/TT computers and some clones. ICD-aware drivers prepend a $1F byte in front
    // of the CDB (effectively resulting in a custom SCSI command) in order to get access to the full SCSI
    // command set. Native ACSI is limited to the low SCSI command classes with command bytes < $20.
    // Most other host adapters (e.g. LINK96/97 and the one by Inventronik) and also several devices (e.g.
    // UltraSatan or GigaFile) that can directly be connected to the Atari's ACSI port also support ICD
    // semantics. I fact, these semantics have become a standard in the Atari world.

    // RaSCSI becomes ICD compatible by ignoring the prepended $1F byte before processing the CDB.
    if (*buf == 0x1F) {
        SetSignal(PIN_REQ, ON);

        ret = WaitSignal(PIN_ACK, ON);

        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

        // Get the actual SCSI command
        *buf = GetDAT();

        SetSignal(PIN_REQ, OFF);

        if (!ret) {
            EnableIRQ();
            return 0;
        }

        WaitSignal(PIN_ACK, OFF);

        if (!ret) {
            EnableIRQ();
            return 0;
        }
    }

    // Increment buffer pointer
    buf++;

    int bytes_received;
    for (bytes_received = 1; bytes_received < command_byte_count; bytes_received++) {
        // Assert REQ signal
        SetSignal(PIN_REQ, ON);

        // Wait for ACK signal
        ret = WaitSignal(PIN_ACK, ON);

        // Wait until the signal line stabilizes
        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

        // Get data
        *buf = GetDAT();

        // Clear the REQ signal
        SetSignal(PIN_REQ, OFF);

        // Check for timeout waiting for ACK assertion
        if (!ret) {
            break;
        }

        // Wait for ACK to clear
        ret = WaitSignal(PIN_ACK, OFF);

        // Check for timeout waiting for ACK to clear
        if (!ret) {
            break;
        }

        // Advance the buffer pointer to receive the next byte
        buf++;
    }

    EnableIRQ();

    return bytes_received;
}

//---------------------------------------------------------------------------
//
//	Data reception handshake
//
//---------------------------------------------------------------------------
int GPIOBUS::ReceiveHandShake(uint8_t *buf, int count)
{
    GPIO_FUNCTION_TRACE

    int i;

    // Disable IRQs
    DisableIRQ();

    if (actmode == mode_e::TARGET) {
        for (i = 0; i < count; i++) {
            // Assert the REQ signal
            SetSignal(PIN_REQ, ON);

            // Wait for ACK
            bool ret = WaitSignal(PIN_ACK, ON);

            // Wait until the signal line stabilizes
            SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

            // Get data
            *buf = GetDAT();

            // Clear the REQ signal
            SetSignal(PIN_REQ, OFF);

            // Check for timeout waiting for ACK signal
            if (!ret) {
                break;
            }

            // Wait for ACK to clear
            ret = WaitSignal(PIN_ACK, OFF);

            // Check for timeout waiting for ACK to clear
            if (!ret) {
                break;
            }

            // Advance the buffer pointer to receive the next byte
            buf++;
        }
    } else {
        // Get phase
        uint32_t phase = Acquire() & GPIO_MCI;

        for (i = 0; i < count; i++) {
            // Wait for the REQ signal to be asserted
            bool ret = WaitSignal(PIN_REQ, ON);

            // Check for timeout waiting for REQ signal
            if (!ret) {
                break;
            }

            // Phase error
            if ((signals & GPIO_MCI) != phase) {
                break;
            }

            // Wait until the signal line stabilizes
            SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

            // Get data
            *buf = GetDAT();

            // Assert the ACK signal
            SetSignal(PIN_ACK, ON);

            // Wait for REQ to clear
            ret = WaitSignal(PIN_REQ, OFF);

            // Clear the ACK signal
            SetSignal(PIN_ACK, OFF);

            // Check for timeout waiting for REQ to clear
            if (!ret) {
                break;
            }

            // Phase error
            if ((signals & GPIO_MCI) != phase) {
                break;
            }

            // Advance the buffer pointer to receive the next byte
            buf++;
        }
    }

    // Re-enable IRQ
    EnableIRQ();

    // Return the number of bytes received
    return i;
}

//---------------------------------------------------------------------------
//
//	Data transmission handshake
//
//---------------------------------------------------------------------------
int GPIOBUS::SendHandShake(uint8_t *buf, int count, int delay_after_bytes)
{
    GPIO_FUNCTION_TRACE

    int i;

    // Disable IRQs
    DisableIRQ();

    if (actmode == mode_e::TARGET) {
        for (i = 0; i < count; i++) {
            if (i == delay_after_bytes) {
                LOGTRACE("%s DELAYING for %dus after %d bytes", __PRETTY_FUNCTION__, SCSI_DELAY_SEND_DATA_DAYNAPORT_US,
                         (int)delay_after_bytes)
                SysTimer::SleepUsec(SCSI_DELAY_SEND_DATA_DAYNAPORT_US);
            }

            // Set the DATA signals
            SetDAT(*buf);

            // Wait for ACK to clear
            bool ret = WaitSignal(PIN_ACK, OFF);

            // Check for timeout waiting for ACK to clear
            if (!ret) {
                break;
            }

            // Already waiting for ACK to clear

            // Assert the REQ signal
            SetSignal(PIN_REQ, ON);

            // Wait for ACK
            ret = WaitSignal(PIN_ACK, ON);

            // Clear REQ signal
            SetSignal(PIN_REQ, OFF);

            // Check for timeout waiting for ACK to clear
            if (!ret) {
                break;
            }

            // Advance the data buffer pointer to receive the next byte
            buf++;
        }

        // Wait for ACK to clear
        WaitSignal(PIN_ACK, OFF);
    } else {
        // Get Phase
        uint32_t phase = Acquire() & GPIO_MCI;

        for (i = 0; i < count; i++) {
            if (i == delay_after_bytes) {
                LOGTRACE("%s DELAYING for %dus after %d bytes", __PRETTY_FUNCTION__, SCSI_DELAY_SEND_DATA_DAYNAPORT_US,
                         (int)delay_after_bytes)
                SysTimer::SleepUsec(SCSI_DELAY_SEND_DATA_DAYNAPORT_US);
            }

            // Set the DATA signals
            SetDAT(*buf);

            // Wait for REQ to be asserted
            bool ret = WaitSignal(PIN_REQ, ON);

            // Check for timeout waiting for REQ to be asserted
            if (!ret) {
                break;
            }

            // Phase error
            if ((signals & GPIO_MCI) != phase) {
                break;
            }

            // Already waiting for REQ assertion

            // Assert the ACK signal
            SetSignal(PIN_ACK, ON);

            // Wait for REQ to clear
            ret = WaitSignal(PIN_REQ, OFF);

            // Clear the ACK signal
            SetSignal(PIN_ACK, OFF);

            // Check for timeout waiting for REQ to clear
            if (!ret) {
                break;
            }

            // Phase error
            if ((signals & GPIO_MCI) != phase) {
                break;
            }

            // Advance the data buffer pointer to receive the next byte
            buf++;
        }
    }

    // Re-enable IRQ
    EnableIRQ();

    // Return number of transmissions
    return i;
}

#ifdef USE_SEL_EVENT_ENABLE
//---------------------------------------------------------------------------
//
//	SEL signal event polling
//
//---------------------------------------------------------------------------
bool GPIOBUS::PollSelectEvent()
{
    GPIO_FUNCTION_TRACE
    errno = 0;

    if (epoll_event epev; epoll_wait(epfd, &epev, 1, -1) <= 0) {
        LOGWARN("%s epoll_wait failed", __PRETTY_FUNCTION__)
        LOGWARN("[%08X] %s", errno, strerror(errno))
        return false;
    }

    if (gpioevent_data gpev; read(selevreq.fd, &gpev, sizeof(gpev)) < 0) {
        LOGWARN("%s read failed", __PRETTY_FUNCTION__)
        LOGWARN("[%08X] %s", errno, strerror(errno))
        return false;
    }

    return true;
}

//---------------------------------------------------------------------------
//
//	Cancel SEL signal event
//
//---------------------------------------------------------------------------
void GPIOBUS::ClearSelectEvent()
{
    GPIO_FUNCTION_TRACE
}
#endif // USE_SEL_EVENT_ENABLE

//---------------------------------------------------------------------------
//
//	Signal table
//
//---------------------------------------------------------------------------
const array<int, 19> GPIOBUS::SignalTable = {PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6,
                                             PIN_DT7, PIN_DP,  PIN_SEL, PIN_ATN, PIN_RST, PIN_ACK, PIN_BSY,
                                             PIN_MSG, PIN_CD,  PIN_IO,  PIN_REQ, -1};

//---------------------------------------------------------------------------
//
//	Create work table
//
//---------------------------------------------------------------------------
void GPIOBUS::MakeTable(void)
{
    GPIO_FUNCTION_TRACE

    const array<int, 9> pintbl = {PIN_DT0, PIN_DT1, PIN_DT2, PIN_DT3, PIN_DT4, PIN_DT5, PIN_DT6, PIN_DT7, PIN_DP};

    array<bool, 256> tblParity;

    // Create parity table
    for (uint32_t i = 0; i < 0x100; i++) {
        uint32_t bits   = i;
        uint32_t parity = 0;
        for (int j = 0; j < 8; j++) {
            parity ^= bits & 1;
            bits >>= 1;
        }
        parity       = ~parity;
        tblParity[i] = parity & 1;
    }

#if SIGNAL_CONTROL_MODE == 0
    // Mask and setting data generation
    for (auto &tbl : tblDatMsk) {
        tbl.fill(-1);
    }
    for (auto &tbl : tblDatSet) {
        tbl.fill(0);
    }

    for (uint32_t i = 0; i < 0x100; i++) {
        // Bit string for inspection
        uint32_t bits = i;

        // Get parity
        if (tblParity[i]) {
            bits |= (1 << 8);
        }

        // Bit check
        for (int j = 0; j < 9; j++) {
            // Index and shift amount calculation
            int index = pintbl[j] / 10;
            int shift = (pintbl[j] % 10) * 3;

            // Mask data
            tblDatMsk[index][i] &= ~(0x7 << shift);

            // Setting data
            if (bits & 1) {
                tblDatSet[index][i] |= (1 << shift);
            }

            bits >>= 1;
        }
    }
#else
    for (uint32_t i = 0; i < 0x100; i++) {
        // Bit string for inspection
        uint32_t bits = i;

        // Get parity
        if (tblParity[i]) {
            bits |= (1 << 8);
        }

#if SIGNAL_CONTROL_MODE == 1
        // Negative logic is inverted
        bits = ~bits;
#endif

        // Create GPIO register information
        uint32_t gpclr = 0;
        uint32_t gpset = 0;
        for (int j = 0; j < 9; j++) {
            if (bits & 1) {
                gpset |= (1 << pintbl[j]);
            } else {
                gpclr |= (1 << pintbl[j]);
            }
            bits >>= 1;
        }

        tblDatMsk[i] = gpclr;
        tblDatSet[i] = gpset;
    }
#endif
}

//---------------------------------------------------------------------------
//
//	Wait for signal change
//
//---------------------------------------------------------------------------
bool GPIOBUS::WaitSignal(int pin, int ast)
{
    GPIO_FUNCTION_TRACE

    // Get current time
    uint32_t now = SysTimer::GetTimerLow();

    // Calculate timeout (3000ms)
    uint32_t timeout = 3000 * 1000;

    // end immediately if the signal has changed
    do {
        // Immediately upon receiving a reset
        Acquire();
        if (GetRST()) {
            return false;
        }

        // Check for the signal edge
        if (((signals >> pin) ^ ~ast) & 1) {
            return true;
        }
    } while ((SysTimer::GetTimerLow() - now) < timeout);

    // We timed out waiting for the signal
    return false;
}

//---------------------------------------------------------------------------
//
//	Generic Phase Acquisition (Doesn't read GPIO)
//
//---------------------------------------------------------------------------
BUS::phase_t GPIOBUS::GetPhaseRaw(uint32_t raw_data)
{
    GPIO_FUNCTION_TRACE
    // Selection Phase
    if (GetPinRaw(raw_data, PIN_SEL)) {
        if (GetPinRaw(raw_data, PIN_IO)) {
            return BUS::phase_t::reselection;
        } else {
            return BUS::phase_t::selection;
        }
    }

    // Bus busy phase
    if (!GetPinRaw(raw_data, PIN_BSY)) {
        return BUS::phase_t::busfree;
    }

    // Get target phase from bus signal line
    int mci = GetPinRaw(raw_data, PIN_MSG) ? 0x04 : 0x00;
    mci |= GetPinRaw(raw_data, PIN_CD) ? 0x02 : 0x00;
    mci |= GetPinRaw(raw_data, PIN_IO) ? 0x01 : 0x00;
    return GetPhase(mci);
}
