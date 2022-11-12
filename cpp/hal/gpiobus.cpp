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
#include "hal/sbc_version.h"
#include "hal/systimer.h"
#include "shared/log.h"
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

using namespace std;


bool GPIOBUS::Init(mode_e mode)
{
    GPIO_FUNCTION_TRACE


    // Save operation mode
    actmode = mode;

    // SignalTable.push_back(board->pin_dt0);
    // SignalTable.push_back(board->pin_dt1);
    // SignalTable.push_back(board->pin_dt2);
    // SignalTable.push_back(board->pin_dt3);
    // SignalTable.push_back(board->pin_dt4);
    // SignalTable.push_back(board->pin_dt5);
    // SignalTable.push_back(board->pin_dt6);
    // SignalTable.push_back(board->pin_dt7);
    // SignalTable.push_back(board->pin_dp);
    // SignalTable.push_back(board->pin_sel);
    // SignalTable.push_back(board->pin_atn);
    // SignalTable.push_back(board->pin_rst);
    // SignalTable.push_back(board->pin_ack);
    // SignalTable.push_back(board->pin_bsy);
    // SignalTable.push_back(board->pin_msg);
    // SignalTable.push_back(board->pin_cd);
    // SignalTable.push_back(board->pin_io);
    // SignalTable.push_back(board->pin_req);

    return true;
}

// // The GPIO hardware needs to be initialized before calling this function.
// void GPIOBUS::InitializeGpio()
// {
//     // Set pull up/pull down
//     LOGTRACE("%s Set pull up/down....", __PRETTY_FUNCTION__);
//     board_type::gpio_pull_up_down_e pullmode;
//     if (board->signal_control_mode == 0) {
//         // #if SIGNAL_CONTROL_MODE == 0
//         LOGTRACE("%s GPIO_PULLNONE", __PRETTY_FUNCTION__);
//         pullmode = board_type::gpio_pull_up_down_e::GPIO_PULLNONE;
//     } else if (board->signal_control_mode == 1) {
//         // #elif SIGNAL_CONTROL_MODE == 1
//         LOGTRACE("%s GPIO_PULLUP", __PRETTY_FUNCTION__);
//         pullmode = board_type::gpio_pull_up_down_e ::GPIO_PULLUP;
//     } else {
//         // #else
//         LOGTRACE("%s GPIO_PULLDOWN", __PRETTY_FUNCTION__);
//         pullmode = board_type::gpio_pull_up_down_e ::GPIO_PULLDOWN;
//     }
//     // #endif

//     // Initialize all signals
//     LOGTRACE("%s Initialize all signals....", __PRETTY_FUNCTION__);

//     for (auto cur_signal : SignalTable) {
//         PinSetSignal(cur_signal, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//         PinConfig(cur_signal, board_type::gpio_direction_e::GPIO_INPUT);
//         PullConfig(cur_signal, pullmode);
//     }

//     // Set control signals
//     LOGTRACE("%s Set control signals....", __PRETTY_FUNCTION__);
//     PinSetSignal(board->pin_act, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//     PinSetSignal(board->pin_tad, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//     PinSetSignal(board->pin_ind, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//     PinSetSignal(board->pin_dtd, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//     PinConfig(board->pin_act, board_type::gpio_direction_e::GPIO_OUTPUT);
//     PinConfig(board->pin_tad, board_type::gpio_direction_e::GPIO_OUTPUT);
//     PinConfig(board->pin_ind, board_type::gpio_direction_e::GPIO_OUTPUT);
//     PinConfig(board->pin_dtd, board_type::gpio_direction_e::GPIO_OUTPUT);

//     // Set the ENABLE signal
//     // This is used to show that the application is running
//     PinConfig(board->pin_enb, board_type::gpio_direction_e::GPIO_OUTPUT);
//     PinSetSignal(board->pin_enb, board->EnbOn());

// }


// void GPIOBUS::Cleanup()
// {
// #if defined(__x86_64__) || defined(__X86__)
// 	return;
// #else
// 	// Release SEL signal interrupt
// #ifdef USE_SEL_EVENT_ENABLE
// 	close(selevreq.fd);
// #endif	// USE_SEL_EVENT_ENABLE

//     // Set control signals
//     PinSetSignal(board->pin_enb, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//     PinSetSignal(board->pin_act, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//     PinSetSignal(board->pin_tad, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//     PinSetSignal(board->pin_ind, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//     PinSetSignal(board->pin_dtd, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//     PinConfig(board->pin_act, board_type::gpio_direction_e::GPIO_INPUT);
//     PinConfig(board->pin_tad, board_type::gpio_direction_e::GPIO_INPUT);
//     PinConfig(board->pin_ind, board_type::gpio_direction_e::GPIO_INPUT);
//     PinConfig(board->pin_dtd, board_type::gpio_direction_e::GPIO_INPUT);

//     // Initialize all signals
//     for (board_type::pi_physical_pin_e pin : SignalTable) {
//         if (pin != board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE) {
//             PinSetSignal(pin, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//             PinConfig(pin, board_type::gpio_direction_e::GPIO_INPUT);
//             PullConfig(pin, board_type::gpio_pull_up_down_e::GPIO_PULLNONE);
//         }
//     }

//     // Set drive strength back to 8mA
//     DrvConfig(3);
// #endif // ifdef __x86_64__ || __X86__
// }

// void GPIOBUS::Reset()
// {
//     GPIO_FUNCTION_TRACE
// #if defined(__x86_64__) || defined(__X86__)
//     return;
// #else

//     // Turn off active signal
//     SetControl(board->pin_act, board->ActOff());

//     for (board_type::pi_physical_pin_e pin : SignalTable) {
//         if (pin != board_type::pi_physical_pin_e::PI_PHYS_PIN_NONE) {
//             SetSignal(pin, board_type::gpio_high_low_e::GPIO_STATE_LOW);
//         }
//     }

//     if (actmode == mode_e::TARGET) {
//         // Target mode

//         // Set target signal to input
//         SetControl(board->pin_tad, board->TadIn());
//         SetMode(board->pin_bsy, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_msg, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_cd, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_req, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_io, board_type::gpio_direction_e::GPIO_INPUT);

//         // Set the initiator signal to input
//         SetControl(board->pin_ind, board->IndIn());
//         SetMode(board->pin_sel, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_atn, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_ack, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_rst, board_type::gpio_direction_e::GPIO_INPUT);

//         // Set data bus signals to input
//         SetControl(board->pin_dtd, board->DtdIn());
//         SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_INPUT);
//     } else {
//         // Initiator mode

//         // Set target signal to input
//         SetControl(board->pin_tad, board->TadIn());
//         SetMode(board->pin_bsy, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_msg, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_cd, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_req, board_type::gpio_direction_e::GPIO_INPUT);
//         SetMode(board->pin_io, board_type::gpio_direction_e::GPIO_INPUT);

//         // Set the initiator signal to output
//         SetControl(board->pin_ind, board->IndOut());
//         SetMode(board->pin_sel, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_atn, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_ack, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_rst, board_type::gpio_direction_e::GPIO_OUTPUT);

//         // Set the data bus signals to output
//         SetControl(board->pin_dtd, board->DtdOut());
//         SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_OUTPUT);
//         SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_OUTPUT);
//     }

//     // Re-read all signals
//     Acquire();
//     // signals = 0;
// #endif // ifdef __x86_64__ || __X86__
// }

// void GPIOBUS::SetENB(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     PinSetSignal(board->pin_enb, ast ? board->EnbOn() : board->EnbOff());
// }

// bool GPIOBUS::GetBSY() const
// {
//     GPIO_FUNCTION_TRACE
//     return GetSignal(board->pin_bsy);
// }

// void GPIOBUS::SetBSY(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     // Set BSY signal
//     SetSignal(board->pin_bsy, board->bool_to_gpio_state(ast));

//     if (actmode == mode_e::TARGET) {
//         if (ast) {
//             // Turn on ACTIVE signal
//             SetControl(board->pin_act, board->act_on);

//             // Set Target signal to output
//             SetControl(board->pin_tad, board->TadOut());

//             SetMode(board->pin_bsy, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_msg, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_cd, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_req, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_io, board_type::gpio_direction_e::GPIO_OUTPUT);
//         } else {
//             // Turn off the ACTIVE signal
//             SetControl(board->pin_act, board->ActOff());

//             // Set the target signal to input
//             SetControl(board->pin_tad, board->TadIn());

//             SetMode(board->pin_bsy, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_msg, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_cd, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_req, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_io, board_type::gpio_direction_e::GPIO_INPUT);
//         }
//     }
// }

// bool GPIOBUS::GetSEL() const
// {
//     GPIO_FUNCTION_TRACE
//     return GetSignal(board->pin_sel);
// }

// void GPIOBUS::SetSEL(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     if (actmode == mode_e::INITIATOR && ast) {
//         // Turn on ACTIVE signal
//         SetControl(board->pin_act, board->act_on);
//     }

//     // Set SEL signal
//     SetSignal(board->pin_sel, board->bool_to_gpio_state(ast));
// }

// bool GPIOBUS::GetATN() const
// {
//     GPIO_FUNCTION_TRACE
//     return GetSignal(board->pin_atn);
// }

// void GPIOBUS::SetATN(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     SetSignal(board->pin_atn, board->bool_to_gpio_state(ast));
// }

// bool GPIOBUS::GetACK() const
// {
//     GPIO_FUNCTION_TRACE
//     return GetSignal(board->pin_ack);
// }

// void GPIOBUS::SetACK(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     SetSignal(board->pin_ack, board->bool_to_gpio_state(ast));
// }

// bool GPIOBUS::GetACT() const
// {
//     GPIO_FUNCTION_TRACE
//     return GetSignal(board->pin_act);
// }

// void GPIOBUS::SetACT(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     SetSignal(board->pin_act, (ast) ? board->ActOn() : board->ActOff());
// }

// bool GPIOBUS::GetRST() const
// {
//     GPIO_FUNCTION_TRACE
//     return GetSignal(board->pin_rst);
// }

// void GPIOBUS::SetRST(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     SetSignal(board->pin_rst, (ast) ? board->act_on : board->ActOff());
// }

// bool GPIOBUS::GetMSG() const
// {
//     GPIO_FUNCTION_TRACE
//     return GetSignal(board->pin_msg);
// }

// void GPIOBUS::SetMSG(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     SetSignal(board->pin_msg, board->bool_to_gpio_state(ast));
// }

// bool GPIOBUS::GetCD() const
// {
//     GPIO_FUNCTION_TRACE
//     return GetSignal(board->pin_cd);
// }

// void GPIOBUS::SetCD(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     SetSignal(board->pin_cd, board->bool_to_gpio_state(ast));
// }

// bool GPIOBUS::GetIO()
// {
//     GPIO_FUNCTION_TRACE
//     bool ast = GetSignal(board->pin_io);

//     if (actmode == mode_e::INITIATOR) {
//         // Change the data input/output direction by IO signal
//         if (ast) {
//             SetControl(board->pin_dtd, board->DtdIn());
//             SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_INPUT);
//         } else {
//             SetControl(board->pin_dtd, board->DtdOut());
//             SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_OUTPUT);
//         }
//     }

//     return ast;
// }

// void GPIOBUS::SetIO(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     SetSignal(board->pin_io, board->bool_to_gpio_state(ast));

//     if (actmode == mode_e::TARGET) {
//         // Change the data input/output direction by IO signal
//         if (ast) {
//             SetControl(board->pin_dtd, board->DtdOut());
//             SetDAT(0);
//             SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_OUTPUT);
//             SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_OUTPUT);
//         } else {
//             SetControl(board->pin_dtd, board->DtdIn());
//             SetMode(board->pin_dt0, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt1, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt2, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt3, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt4, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt5, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt6, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dt7, board_type::gpio_direction_e::GPIO_INPUT);
//             SetMode(board->pin_dp, board_type::gpio_direction_e::GPIO_INPUT);
//         }
//     }
// }

// bool GPIOBUS::GetREQ() const
// {
//     GPIO_FUNCTION_TRACE
//     return GetSignal(board->pin_req);
// }

// void GPIOBUS::SetREQ(bool ast)
// {
//     GPIO_FUNCTION_TRACE
//     SetSignal(board->pin_req, board->bool_to_gpio_state(ast));
// }



// bool GPIOBUS::GetDP() const
// {
// 	return GetSignal(PIN_DP);
// }

// // Extract as specific pin field from a raw data capture
// uint32_t GPIOBUS::GetPinRaw(uint32_t raw_data, board_type::pi_physical_pin_e pin_num)
// {
//     // return GetSignalRaw(raw_data, pin_num);
//     (void)raw_data;
//     (void)pin_num;
//     LOGWARN("THIS FUNCTION ISN'T IMPLEMENTED YET");
//     return 0;
// }




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
	SetSignal(PIN_REQ, ON);

	// Wait for ACK signal
	bool ret = WaitSignal(PIN_ACK, ON);

	// Wait until the signal line stabilizes
	SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

	// Get data
	buf[0] = GetDAT();

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
	if (buf[0] == 0x1F) {
		SetSignal(PIN_REQ, ON);

		ret = WaitSignal(PIN_ACK, ON);

		SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

		// Get the actual SCSI command
		buf[0] = GetDAT();

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
		SetSignal(PIN_REQ, ON);

		// Wait for ACK signal
		ret = WaitSignal(PIN_ACK, ON);

		// Wait until the signal line stabilizes
		SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

		// Get data
		buf[offset] = GetDAT();

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
        Acquire();
		BUS::phase_t phase = GetPhase();

		for (i = 0; i < count; i++) {
			// Wait for the REQ signal to be asserted
			bool ret = WaitSignal(PIN_REQ, ON);

			// Check for timeout waiting for REQ signal
			if (!ret) {
				break;
			}

			// Phase error
            Acquire();
			if (GetPhase() != phase) {
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
            Acquire();
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
			if(i==delay_after_bytes){
				LOGTRACE("%s DELAYING for %dus after %d bytes", __PRETTY_FUNCTION__, SCSI_DELAY_SEND_DATA_DAYNAPORT_US, (int)delay_after_bytes)
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
        Acquire();
		BUS::phase_t phase = GetPhase();

		for (i = 0; i < count; i++) {
			if(i==delay_after_bytes){
				LOGTRACE("%s DELAYING for %dus after %d bytes", __PRETTY_FUNCTION__, SCSI_DELAY_SEND_DATA_DAYNAPORT_US, (int)delay_after_bytes)
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
            Acquire();
			if (GetPhase() != phase) {
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
		return false;
	}

	if (gpioevent_data gpev; read(selevreq.fd, &gpev, sizeof(gpev)) < 0) {
		LOGWARN("%s read failed", __PRETTY_FUNCTION__)
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
#endif	// USE_SEL_EVENT_ENABLE



//---------------------------------------------------------------------------
//
//	Wait for signal change
//
//---------------------------------------------------------------------------
bool GPIOBUS::WaitSignal(int pin, bool ast)
{
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
        if ((GetSignal(pin) ^ !ast) & 1) {
			return true;
		}
	} while ((SysTimer::GetTimerLow() - now) < timeout);

	// We timed out waiting for the signal
	return false;
}