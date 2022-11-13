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

// #if defined CONNECT_TYPE_STANDARD
// #include "hal/gpiobus_standard.h"
// #elif defined CONNECT_TYPE_FULLSPEC
// #include "hal/gpiobus_fullspec.h"
// #elif defined CONNECT_TYPE_AIBOM
// #include "hal/gpiobus_aibom.h"
// #elif defined CONNECT_TYPE_GAMERNIUM
// #include "hal/gpiobus_gamernium.h"
// #else
// #error Invalid connection type or none specified
// #endif

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
bool GPIOBUS::Init(mode_e mode, board_type::rascsi_board_type_e rascsi_type)
{
    GPIO_FUNCTION_TRACE

    // Figure out what type of board is connected.
    board = board_type::BoardType::GetRascsiBoardSettings(rascsi_type);
    LOGINFO("Connection type: %s", board->connect_desc.c_str());

    // Save operation mode
    actmode = mode;

    SignalTable.push_back(board->pin_dt0);
    SignalTable.push_back(board->pin_dt1);
    SignalTable.push_back(board->pin_dt2);
    SignalTable.push_back(board->pin_dt3);
    SignalTable.push_back(board->pin_dt4);
    SignalTable.push_back(board->pin_dt5);
    SignalTable.push_back(board->pin_dt6);
    SignalTable.push_back(board->pin_dt7);
    SignalTable.push_back(board->pin_dp);
    SignalTable.push_back(board->pin_sel);
    SignalTable.push_back(board->pin_atn);
    SignalTable.push_back(board->pin_rst);
    SignalTable.push_back(board->pin_ack);
    SignalTable.push_back(board->pin_bsy);
    SignalTable.push_back(board->pin_msg);
    SignalTable.push_back(board->pin_cd);
    SignalTable.push_back(board->pin_io);
    SignalTable.push_back(board->pin_req);

    return true;
}

// The GPIO hardware needs to be initialized before calling this function.
void GPIOBUS::InitializeGpio()
{
    // Set pull up/pull down
    LOGTRACE("%s Set pull up/down....", __PRETTY_FUNCTION__);
    board_type::gpio_pull_up_down_e pullmode;
    if (board->signal_control_mode == 0) {
        // #if SIGNAL_CONTROL_MODE == 0
        LOGTRACE("%s GPIO_PULLNONE", __PRETTY_FUNCTION__);
        pullmode = board_type::gpio_pull_up_down_e::GPIO_PULLNONE;
    } else if (board->signal_control_mode == 1) {
        // #elif SIGNAL_CONTROL_MODE == 1
        LOGTRACE("%s GPIO_PULLUP", __PRETTY_FUNCTION__);
        pullmode = board_type::gpio_pull_up_down_e ::GPIO_PULLUP;
    } else {
        // #else
        LOGTRACE("%s GPIO_PULLDOWN", __PRETTY_FUNCTION__);
        pullmode = board_type::gpio_pull_up_down_e ::GPIO_PULLDOWN;
    }
    // #endif

    // Initialize all signals
    LOGTRACE("%s Initialize all signals....", __PRETTY_FUNCTION__);

    for (auto cur_signal : SignalTable) {
        PinSetSignal(cur_signal, board_type::gpio_high_low_e::GPIO_STATE_LOW);
        PinConfig(cur_signal, board_type::gpio_direction_e::GPIO_INPUT);
        PullConfig(cur_signal, pullmode);
    }

    // Set control signals
    LOGTRACE("%s Set control signals....", __PRETTY_FUNCTION__);
    PinSetSignal(board->pin_act, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_tad, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_ind, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_dtd, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinConfig(board->pin_act, board_type::gpio_direction_e::GPIO_OUTPUT);
    PinConfig(board->pin_tad, board_type::gpio_direction_e::GPIO_OUTPUT);
    PinConfig(board->pin_ind, board_type::gpio_direction_e::GPIO_OUTPUT);
    PinConfig(board->pin_dtd, board_type::gpio_direction_e::GPIO_OUTPUT);

    // Set the ENABLE signal
    // This is used to show that the application is running
    PinConfig(board->pin_enb, board_type::gpio_direction_e::GPIO_OUTPUT);
    PinSetSignal(board->pin_enb, board->EnbOn());

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
    PinSetSignal(board->pin_enb, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_act, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_tad, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_ind, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinSetSignal(board->pin_dtd, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    PinConfig(board->pin_act, board_type::gpio_direction_e::GPIO_INPUT);
    PinConfig(board->pin_tad, board_type::gpio_direction_e::GPIO_INPUT);
    PinConfig(board->pin_ind, board_type::gpio_direction_e::GPIO_INPUT);
    PinConfig(board->pin_dtd, board_type::gpio_direction_e::GPIO_INPUT);

    // Initialize all signals
    for (board_type::pi_physical_pin_e pin : SignalTable) {
        if (pin != board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE) {
            PinSetSignal(pin, board_type::gpio_high_low_e::GPIO_STATE_LOW);
            PinConfig(pin, board_type::gpio_direction_e::GPIO_INPUT);
            PullConfig(pin, board_type::gpio_pull_up_down_e::GPIO_PULLNONE);
        }
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

    // Turn off active signal
    SetControl(board->pin_act, board->ActOff());

    for (board_type::pi_physical_pin_e pin : SignalTable) {
        if (pin != board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE) {
            SetSignal(pin, board_type::gpio_high_low_e::GPIO_STATE_LOW);
        }
    }

    if (actmode == mode_e::TARGET) {
        // Target mode

        // Set target signal to input
        SetControl(board->pin_tad, board->TadIn());
        SetMode(board->pin_bsy, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_msg, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_cd, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_req, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_io, board_type::gpio_direction_e::GPIO_INPUT);

        // Set the initiator signal to input
        SetControl(board->pin_ind, board->IndIn());
        SetMode(board->pin_sel, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_atn, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_ack, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_rst, board_type::gpio_direction_e::GPIO_INPUT);

        // Set data bus signals to input
        SetControl(board->pin_dtd, board->DtdIn());
        SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_INPUT);
    } else {
        // Initiator mode

        // Set target signal to input
        SetControl(board->pin_tad, board->TadIn());
        SetMode(board->pin_bsy, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_msg, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_cd, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_req, board_type::gpio_direction_e::GPIO_INPUT);
        SetMode(board->pin_io, board_type::gpio_direction_e::GPIO_INPUT);

        // Set the initiator signal to output
        SetControl(board->pin_ind, board->IndOut());
        SetMode(board->pin_sel, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_atn, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_ack, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_rst, board_type::gpio_direction_e::GPIO_OUTPUT);

        // Set the data bus signals to output
        SetControl(board->pin_dtd, board->DtdOut());
        SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_OUTPUT);
        SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_OUTPUT);
    }

    // Re-read all signals
    Acquire();
    // signals = 0;
#endif // ifdef __x86_64__ || __X86__
}

void GPIOBUS::SetENB(bool ast)
{
    GPIO_FUNCTION_TRACE
    PinSetSignal(board->pin_enb, ast ? board->EnbOn() : board->EnbOff());
}

bool GPIOBUS::GetBSY() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_bsy);
}

void GPIOBUS::SetBSY(bool ast)
{
    GPIO_FUNCTION_TRACE
    // Set BSY signal
    SetSignal(board->pin_bsy, board->bool_to_gpio_state(ast));

    if (actmode == mode_e::TARGET) {
        if (ast) {
            // Turn on ACTIVE signal
            SetControl(board->pin_act, board->act_on);

            // Set Target signal to output
            SetControl(board->pin_tad, board->TadOut());

            SetMode(board->pin_bsy, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_msg, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_cd, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_req, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_io, board_type::gpio_direction_e::GPIO_OUTPUT);
        } else {
            // Turn off the ACTIVE signal
            SetControl(board->pin_act, board->ActOff());

            // Set the target signal to input
            SetControl(board->pin_tad, board->TadIn());

            SetMode(board->pin_bsy, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_msg, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_cd, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_req, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_io, board_type::gpio_direction_e::GPIO_INPUT);
        }
    }
}

bool GPIOBUS::GetSEL() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_sel);
}

void GPIOBUS::SetSEL(bool ast)
{
    GPIO_FUNCTION_TRACE
    if (actmode == mode_e::INITIATOR && ast) {
        // Turn on ACTIVE signal
        SetControl(board->pin_act, board->act_on);
    }

    // Set SEL signal
    SetSignal(board->pin_sel, board->bool_to_gpio_state(ast));
}

bool GPIOBUS::GetATN() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_atn);
}

void GPIOBUS::SetATN(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(board->pin_atn, board->bool_to_gpio_state(ast));
}

bool GPIOBUS::GetACK() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_ack);
}

void GPIOBUS::SetACK(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(board->pin_ack, board->bool_to_gpio_state(ast));
}

bool GPIOBUS::GetACT() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_act);
}

void GPIOBUS::SetACT(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(board->pin_act, (ast) ? board->ActOn() : board->ActOff());
}

bool GPIOBUS::GetRST() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_rst);
}

void GPIOBUS::SetRST(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(board->pin_rst, (ast) ? board->act_on : board->ActOff());
}

bool GPIOBUS::GetMSG() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_msg);
}

void GPIOBUS::SetMSG(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(board->pin_msg, board->bool_to_gpio_state(ast));
}

bool GPIOBUS::GetCD() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_cd);
}

void GPIOBUS::SetCD(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(board->pin_cd, board->bool_to_gpio_state(ast));
}

bool GPIOBUS::GetIO()
{
    GPIO_FUNCTION_TRACE
    bool ast = GetSignal(board->pin_io);

    if (actmode == mode_e::INITIATOR) {
        // Change the data input/output direction by IO signal
        if (ast) {
            SetControl(board->pin_dtd, board->DtdIn());
            SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_INPUT);
        } else {
            SetControl(board->pin_dtd, board->DtdOut());
            SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_OUTPUT);
        }
    }

    return ast;
}

void GPIOBUS::SetIO(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(board->pin_io, board->bool_to_gpio_state(ast));

    if (actmode == mode_e::TARGET) {
        // Change the data input/output direction by IO signal
        if (ast) {
            SetControl(board->pin_dtd, board->DtdOut());
            SetDAT(0);
            SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_OUTPUT);
            SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_OUTPUT);
        } else {
            SetControl(board->pin_dtd, board->DtdIn());
            SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_INPUT);
            SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_INPUT);
        }
    }
}

bool GPIOBUS::GetREQ() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_req);
}

void GPIOBUS::SetREQ(bool ast)
{
    GPIO_FUNCTION_TRACE
    SetSignal(board->pin_req, board->bool_to_gpio_state(ast));
}

bool GPIOBUS::GetDP() const
{
    GPIO_FUNCTION_TRACE
    return GetSignal(board->pin_dp);
}

// Extract as specific pin field from a raw data capture
uint32_t GPIOBUS::GetPinRaw(uint32_t raw_data, board_type::pi_physical_pin_e pin_num)
{
    // return GetSignalRaw(raw_data, pin_num);
    (void)raw_data;
    (void)pin_num;
    LOGWARN("THIS FUNCTION ISN'T IMPLEMENTED YET");
    return 0;
}

//---------------------------------------------------------------------------
//
//	Receive command handshake
//
//---------------------------------------------------------------------------
int GPIOBUS::CommandHandShake(vector<uint8_t>& buf)
{
    GPIO_FUNCTION_TRACE
    // Only works in TARGET mode
    if (actmode != mode_e::TARGET) {
        return 0;
    }

    DisableIRQ();

    // Assert REQ signal
    SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

    // Wait for ACK signal
    bool ret = WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

    // Wait until the signal line stabilizes
    SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

    // Get data
    buf[0] = GetDAT();

    // Disable REQ signal
    SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_LOW);

    // Timeout waiting for ACK assertion
    if (!ret) {
        EnableIRQ();
        return 0;
    }

    // Wait for ACK to clear
    ret = WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_LOW);

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
    if (buf[0] == 0x1F) {
        SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

        ret = WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

        // Get the actual SCSI command
        buf[0] = GetDAT();

        SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_LOW);

        if (!ret) {
            EnableIRQ();
            return 0;
        }

        WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_LOW);

        if (!ret) {
            EnableIRQ();
            return 0;
        }
    }

    const int command_byte_count = GetCommandByteCount(buf[0]);
    if (command_byte_count == 0) {
    	EnableIRQ();

    	return 0;
    }

    int offset = 0;

    int bytes_received;
    for (bytes_received = 1; bytes_received < command_byte_count; bytes_received++) {
    	++offset;

    	// Assert REQ signal
        SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

        // Wait for ACK signal
        ret = WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

        // Wait until the signal line stabilizes
        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

        // Get data
        buf[offset] = GetDAT();

        // Clear the REQ signal
        SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_LOW);

        // Check for timeout waiting for ACK assertion
        if (!ret) {
            break;
        }

        // Wait for ACK to clear
        ret = WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_LOW);

        // Check for timeout waiting for ACK to clear
        if (!ret) {
            break;
        }
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
            SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

            // Wait for ACK
            bool ret = WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

            // Wait until the signal line stabilizes
            SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

            // Get data
            *buf = GetDAT();

            // Clear the REQ signal
            SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_LOW);

            // Check for timeout waiting for ACK signal
            if (!ret) {
                break;
            }

            // Wait for ACK to clear
            ret = WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_LOW);

            // Check for timeout waiting for ACK to clear
            if (!ret) {
                break;
            }

            // Advance the buffer pointer to receive the next byte
            buf++;
        }
    } else {
        // Get phase
        // uint32_t phase = Acquire() & GPIO_MCI;
        (void)Acquire();
        phase_t phase = GetPhase();

        for (i = 0; i < count; i++) {
            // Wait for the REQ signal to be asserted
            bool ret = WaitSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

            // Check for timeout waiting for REQ signal
            if (!ret) {
                break;
            }

            // Phase error
            (void)Acquire();
            if (GetPhase() != phase) {
                break;
            }

            // Wait until the signal line stabilizes
            SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

            // Get data
            *buf = GetDAT();

            // Assert the ACK signal
            SetSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

            // Wait for REQ to clear
            ret = WaitSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_LOW);

            // Clear the ACK signal
            SetSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_LOW);

            // Check for timeout waiting for REQ to clear
            if (!ret) {
                break;
            }

            // Phase error
            (void)Acquire();
            if (GetPhase() != phase) {
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
            bool ret = WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_LOW);

            // Check for timeout waiting for ACK to clear
            if (!ret) {
                break;
            }

            // Already waiting for ACK to clear

            // Assert the REQ signal
            SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

            // Wait for ACK
            ret = WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

            // Clear REQ signal
            SetSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_LOW);

            // Check for timeout waiting for ACK to clear
            if (!ret) {
                break;
            }

            // Advance the data buffer pointer to receive the next byte
            buf++;
        }

        // Wait for ACK to clear
        WaitSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_LOW);
    } else {
        // Get Phase
        // uint32_t phase = Acquire() & GPIO_MCI;
        (void)Acquire();
        phase_t phase = GetPhase();

        for (i = 0; i < count; i++) {
            if (i == delay_after_bytes) {
                LOGTRACE("%s DELAYING for %dus after %d bytes", __PRETTY_FUNCTION__, SCSI_DELAY_SEND_DATA_DAYNAPORT_US,
                         (int)delay_after_bytes)
                SysTimer::SleepUsec(SCSI_DELAY_SEND_DATA_DAYNAPORT_US);
            }

            // Set the DATA signals
            SetDAT(*buf);

            // Wait for REQ to be asserted
            bool ret = WaitSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

            // Check for timeout waiting for REQ to be asserted
            if (!ret) {
                break;
            }

            // Phase error
            Acquire();
            if (GetPhase() != phase) {
                break;
            }

            // Already waiting for REQ assertion

            // Assert the ACK signal
            SetSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_HIGH);

            // Wait for REQ to clear
            ret = WaitSignal(board->pin_req, board_type::gpio_high_low_e::GPIO_STATE_LOW);

            // Clear the ACK signal
            SetSignal(board->pin_ack, board_type::gpio_high_low_e::GPIO_STATE_LOW);

            // Check for timeout waiting for REQ to clear
            if (!ret) {
                break;
            }

            // Phase error
            Acquire();
            if (GetPhase() != phase) {
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
vector<board_type::pi_physical_pin_e> GPIOBUS::SignalTable;

//---------------------------------------------------------------------------
//
//	Create work table
//
//---------------------------------------------------------------------------
void GPIOBUS::MakeTable(void)
{
    GPIO_FUNCTION_TRACE

    [[maybe_unused]] const array<board_type::pi_physical_pin_e, 9> pintbl = {
        board->pin_dt0, board->pin_dt1, board->pin_dt2, board->pin_dt3, board->pin_dt4,
        board->pin_dt5, board->pin_dt6, board->pin_dt7, board->pin_dp};

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
        // TODO: temporary to make clang++ happy.....
        (void)bits;

        // Get parity
        if (tblParity[i]) {
            bits |= (1 << 8);
        }

        // TODO: THIS NEEDS TO BE MOVED DOWN TO THE BOARD_SPECIFIC REGION
        // // Bit check
        // for (int j = 0; j < 9; j++) {
        //     // Index and shift amount calculation
        //     int index = pintbl[j] / 10;
        //     int shift = (pintbl[j] % 10) * 3;

        //     // Mask data
        //     tblDatMsk[index][i] &= ~(0x7 << shift);

        //     // Setting data
        //     if (bits & 1) {
        //         tblDatSet[index][i] |= (1 << shift);
        //     }

        //     bits >>= 1;
        // }
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
bool GPIOBUS::WaitSignal(board_type::pi_physical_pin_e pin, board_type::gpio_high_low_e ast)
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
        LOGWARN("THIS WONT WORK. NEEDS TO BE UPDATED");

        if ((GetSignal(pin) ^ ~((int)(board->gpio_state_to_bool(ast)))) & 1) {
            return true;
        }

        // if (((signals >> pin) ^ ~ast) & 1) {
        //     return true;
        // }
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
    GPIO_FUNCTION_TRACE(void) raw_data;
    // // Selection Phase
    // if (GetPinRaw(raw_data, board->pin_sel)) {
    //     if (GetPinRaw(raw_data, board->pin_io)) {
    //         return BUS::phase_t::reselection;
    //     } else {
    //         return BUS::phase_t::selection;
    //     }
    // }

    // // Bus busy phase
    // if (!GetPinRaw(raw_data, board->pin_bsy)) {
    //     return BUS::phase_t::busfree;
    // }

    LOGWARN("NOT IMPLEMENTED YET");
    return BUS::phase_t::busfree;
    //     // Selection Phase
    //     if (GetSignal(board->pin_sel)) {
    //         if (GetSignal(board->pin_io)) {
    //             return BUS::phase_t::reselection;
    //         } else {
    //             return BUS::phase_t::selection;
    //         }
    //     }

    //     // Bus busy phase
    //     if (!GetPinRaw(raw_data, board->pin_bsy)) {
    //         return BUS::phase_t::busfree;
    //     }

    //     // Get target phase from bus signal line
    //     int mci = GetPinRaw(raw_data, board->pin_msg) ? 0x04 : 0x00;
    //     mci |= GetPinRaw(raw_data, board->pin_cd) ? 0x02 : 0x00;
    //     mci |= GetPinRaw(raw_data, board->pin_io) ? 0x01 : 0x00;
    //     return GetPhase(mci);
}

const string GPIOBUS::GetConnectDesc()
{
    if (board == nullptr) {
        return "unknown";
    }
    return board->connect_desc;
}
