//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "hal/gpiobus.h"
#include "hal/sbc_version.h"
#include "hal/systimer.h"
#include <spdlog/spdlog.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#include <chrono>

using namespace std;

namespace {

// Keep this experiment scoped to target-mode data reception. The archived
// RaSCSI implementation used a 50 ns settling delay at this point.
constexpr uint32_t TARGET_RECEIVE_SETTLING_DELAY_NS = 50;

struct handshake_failure_t {
    const char *stage = nullptr;
    int byte_index = 0;
    uint32_t signals = 0;
    bool req = false;
    bool ack = false;
    bool rst = false;
};

void LogHandshakeFailure(const char *direction, int count, const handshake_failure_t& failure)
{
    if (failure.stage) {
        spdlog::warn("TEMP GPIO target " + string(direction) + " handshake stopped: stage=" + failure.stage +
            ", byte_index=" + to_string(failure.byte_index) + ", expected=" + to_string(count) +
            ", signals=" + to_string(failure.signals) + ", REQ=" + to_string(failure.req) +
            ", ACK=" + to_string(failure.ack) + ", RST=" + to_string(failure.rst));
    }
}

}

bool GPIOBUS::Init(mode_e mode)
{
    GPIO_FUNCTION_TRACE

    // Save operation mode
    actmode = mode;

    return true;
}

//---------------------------------------------------------------------------
//
//	Receive command handshake
//
//---------------------------------------------------------------------------
int GPIOBUS::CommandHandShake(vector<uint8_t> &buf)
{
    // Only works in TARGET mode
	assert(actmode == mode_e::TARGET);

	GPIO_FUNCTION_TRACE

    DisableIRQ();

    // Assert REQ signal
    SetREQ(ON);

    // Wait for ACK signal
    bool ret = WaitACK(ON);

    // Wait until the signal line stabilizes
    SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

    // Get data
    buf[0] = GetDAT();

    // Disable REQ signal
    SetREQ(OFF);

    // Timeout waiting for ACK assertion
    if (!ret) {
        EnableIRQ();
        return 0;
    }

    // Wait for ACK to clear
    ret = WaitACK(OFF);

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

    // PiSCSI becomes ICD compatible by ignoring the prepended $1F byte before processing the CDB.
    if (buf[0] == 0x1F) {
        SetREQ(ON);

        ret = WaitACK(ON);

        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

        // Get the actual SCSI command
        buf[0] = GetDAT();

        SetREQ(OFF);

        if (!ret) {
            EnableIRQ();
            return 0;
        }

        WaitACK(OFF);

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
        SetREQ(ON);

        // Wait for ACK signal
        ret = WaitACK(ON);

        // Wait until the signal line stabilizes
        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);

        // Get data
        buf[offset] = GetDAT();

        // Clear the REQ signal
        SetREQ(OFF);

        // Check for timeout waiting for ACK assertion
        if (!ret) {
            break;
        }

        // Wait for ACK to clear
        ret = WaitACK(OFF);

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
    handshake_failure_t failure;

    // Disable IRQs
    DisableIRQ();

    if (actmode == mode_e::TARGET) {
        for (i = 0; i < count; i++) {
            // Assert the REQ signal
            SetREQ(ON);

            // Wait for ACK
            bool ret = WaitACK(ON);

            if (!ret) {
                failure = { "ack_assert", i, Acquire(), GetREQ(), GetACK(), GetRST() };
                SetREQ(OFF);
                break;
            }

            // Wait until the signal line stabilizes
            SysTimer::SleepNsec(TARGET_RECEIVE_SETTLING_DELAY_NS);

            // Get data
            *buf = GetDAT();

            // Clear the REQ signal
            SetREQ(OFF);

            // Wait for ACK to clear
            ret = WaitACK(OFF);

            // Check for timeout waiting for ACK to clear
            if (!ret) {
                failure = { "ack_deassert", i, Acquire(), GetREQ(), GetACK(), GetRST() };
                break;
            }

            // Advance the buffer pointer to receive the next byte
            buf++;
        }
    } else {
        // Get phase
        Acquire();
        phase_t phase = GetPhase();

        for (i = 0; i < count; i++) {
            // Wait for the REQ signal to be asserted
            bool ret = WaitREQ(ON);

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
            SetACK(ON);

            // Wait for REQ to clear
            ret = WaitREQ(OFF);

            // Clear the ACK signal
            SetACK(OFF);

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

    if (actmode == mode_e::TARGET) {
        LogHandshakeFailure("receive", count, failure);
    }

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
    handshake_failure_t failure;

    // Disable IRQs
    DisableIRQ();

    if (actmode == mode_e::TARGET) {
        for (i = 0; i < count; i++) {
            if (i == delay_after_bytes) {
                spdlog::trace("DELAYING for " + to_string(SCSI_DELAY_SEND_DATA_DAYNAPORT_NS) + " ns after " +
                		to_string(delay_after_bytes) + " bytes");
                EnableIRQ();
                const timespec ts = { .tv_sec = 0, .tv_nsec = SCSI_DELAY_SEND_DATA_DAYNAPORT_NS};
                nanosleep(&ts, nullptr);
                DisableIRQ();
            }

            // Set the DATA signals
            SetDAT(*buf);

            // Wait for ACK to clear
            bool ret = WaitACK(OFF);

            // Check for timeout waiting for ACK to clear
            if (!ret) {
                failure = { "ack_deassert_before_req", i, Acquire(), GetREQ(), GetACK(), GetRST() };
                break;
            }

            // Already waiting for ACK to clear

            // Assert the REQ signal
            SetREQ(ON);

            // Wait for ACK
            ret = WaitACK(ON);

            if (!ret) {
                failure = { "ack_assert", i, Acquire(), GetREQ(), GetACK(), GetRST() };
                SetREQ(OFF);
                break;
            }
            SetREQ(OFF);

            // Advance the data buffer pointer to receive the next byte
            buf++;
        }

        // Expose a missed final transition that would otherwise be
        // attributed to the next phase.
        const bool final_ack_cleared = WaitACK(OFF);
        if (!failure.stage && !final_ack_cleared) {
            failure = { "final_ack_deassert", i, Acquire(), GetREQ(), GetACK(), GetRST() };
        }
    } else {
        // Get Phase
        Acquire();
        phase_t phase = GetPhase();

        for (i = 0; i < count; i++) {
            // Set the DATA signals
            SetDAT(*buf);

            // Wait for REQ to be asserted
            bool ret = WaitREQ(ON);

            // Check for timeout waiting for REQ to be asserted
            if (!ret) {
                break;
            }

           	// Signal the last MESSAGE OUT byte
            if (phase == phase_t::msgout && i == count - 1) {
            	SetATN(false);
            }

            // Phase error
            Acquire();
            if (GetPhase() != phase) {
                break;
            }

            // Already waiting for REQ assertion

            // Assert the ACK signal
            SetACK(ON);

            // Wait for REQ to clear
            ret = WaitREQ(OFF);

            // Clear the ACK signal
            SetACK(OFF);

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

    if (actmode == mode_e::TARGET) {
        LogHandshakeFailure("send", count, failure);
    }

    // Return number of transmissions
    return i;
}

//---------------------------------------------------------------------------
//
//	SEL signal event polling
//
//---------------------------------------------------------------------------
bool GPIOBUS::PollSelectEvent()
{
#ifndef USE_SEL_EVENT_ENABLE
    return false;
#else
    GPIO_FUNCTION_TRACE
    errno = 0;

    if (epoll_event epev; epoll_wait(epfd, &epev, 1, -1) <= 0) {
        spdlog::warn("epoll_wait failed");
        return false;
    }

    if (gpioevent_data gpev; read(selevreq.fd, &gpev, sizeof(gpev)) < 0) {
        spdlog::warn("read failed");
        return false;
    }

    return true;
#endif
}

bool GPIOBUS::WaitSignal(int pin, bool ast)
{
    const auto now = chrono::steady_clock::now();

    // Wait up to 3 s
    do {
        Acquire();

        if (GetSignal(pin) == ast) {
            return true;
        }

        // Abort on a reset
        if (GetRST()) {
            return false;
        }
    } while ((chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - now).count()) < 3);

    return false;
}
