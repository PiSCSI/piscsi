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
#include <sys/time.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#include <chrono>

using namespace std;

bool GPIOBUS::Init(mode_e mode)
{
    actmode = mode;

    return true;
}

int GPIOBUS::CommandHandShake(vector<uint8_t> &buf)
{
	assert(actmode == mode_e::TARGET);

    DisableIRQ();

    SetREQ(true);

    bool ack = WaitACK(true);

#ifndef SCSI_COMPLIANT_HANDSHAKE
        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);
#endif

    buf[0] = GetDAT();

    SetREQ(false);

    // Timeout waiting for ACK to change
    if (!ack || !WaitACK(false)) {
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
        SetREQ(true);

        ack = WaitACK(true);

#ifndef SCSI_COMPLIANT_HANDSHAKE
        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);
#endif

        // Get the actual SCSI command
        buf[0] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitACK(false)) {
            EnableIRQ();
            return 0;
        }
    }

    const int command_byte_count = GetCommandByteCount(buf[0]);
    if (!command_byte_count) {
        EnableIRQ();

        // Unknown command
        return 0;
    }

    int offset = 0;

    int bytes_received;
    for (bytes_received = 1; bytes_received < command_byte_count; bytes_received++) {
        ++offset;

        SetREQ(true);

        ack = WaitACK(true);

#ifndef SCSI_COMPLIANT_HANDSHAKE
        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);
#endif

        buf[offset] = GetDAT();

        SetREQ(false);

        // Timeout waiting for ACK to change
        if (!ack || !WaitACK(false)) {
            break;
        }
    }

    EnableIRQ();

    return bytes_received;
}

// Handshake for DATA IN and MESSAGE IN
int GPIOBUS::ReceiveHandShake(uint8_t *buf, int count)
{
    int bytes_received;

    DisableIRQ();

    if (actmode == mode_e::TARGET) {
        for (bytes_received = 0; bytes_received < count; bytes_received++) {
            SetREQ(true);

            const bool ack = WaitACK(true);

#ifndef SCSI_COMPLIANT_HANDSHAKE
        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);
#endif

            *buf = GetDAT();

            SetREQ(false);

            // Timeout waiting for ACK to change
            if (!ack || !WaitACK(false)) {
                break;
            }

            buf++;
        }
    } else {
        Acquire();
        const phase_t phase = GetPhase();

        for (bytes_received = 0; bytes_received < count; bytes_received++) {
            // Check for timeout waiting for REQ signal
            if (!WaitREQ(true)) {
                break;
            }

            // Phase error
            Acquire();
            if (GetPhase() != phase) {
                break;
            }

#ifndef SCSI_COMPLIANT_HANDSHAKE
        SysTimer::SleepNsec(SCSI_DELAY_BUS_SETTLE_DELAY_NS);
#endif

            *buf = GetDAT();

            SetACK(true);

            const bool req = WaitREQ(true);

            SetACK(false);

            // Check for timeout waiting for REQ to clear
            if (!req) {
                break;
            }

            // Phase error
            Acquire();
            if (GetPhase() != phase) {
                break;
            }

            buf++;
        }
    }

    EnableIRQ();

    return bytes_received;
}

// Handshake for DATA OUT and MESSAGE OUT
int GPIOBUS::SendHandShake(uint8_t *buf, int count, int delay_after_bytes)
{
    int bytes_sent;

    DisableIRQ();

    if (actmode == mode_e::TARGET) {
        for (bytes_sent = 0; bytes_sent < count; bytes_sent++) {
            if (bytes_sent == delay_after_bytes) {
                spdlog::trace("DELAYING for " + to_string(SCSI_DELAY_SEND_DATA_DAYNAPORT_NS) + " ns after " +
                    to_string(delay_after_bytes) + " bytes");
                EnableIRQ();
                const timespec ts = { .tv_sec = 0, .tv_nsec = SCSI_DELAY_SEND_DATA_DAYNAPORT_NS};
                nanosleep(&ts, nullptr);
                DisableIRQ();
            }

            SetDAT(*buf);

            // Check for timeout waiting for ACK to clear
            if (!WaitACK(false)) {
                break;
            }

            SetREQ(true);

            const bool ack = WaitACK(true);

            SetREQ(false);

            // Check for timeout waiting for ACK to clear
            // TODO Do we really have to clear REQ here and everywhere else before checking this?
            if (!ack) {
                break;
            }

            buf++;
        }

        WaitACK(false);
    } else {
        Acquire();
        const phase_t phase = GetPhase();

        for (bytes_sent = 0; bytes_sent < count; bytes_sent++) {
            SetDAT(*buf);

            // Check for timeout waiting for REQ to be asserted
            if (!WaitREQ(true)) {
                break;
            }

           	// Signal the last MESSAGE OUT byte
            if (phase == phase_t::msgout && bytes_sent == count - 1) {
            	SetATN(false);
            }

            // Phase error
            Acquire();
            if (GetPhase() != phase) {
                break;
            }

            SetACK(true);

            const bool req = WaitREQ(false);

            SetACK(true);

            // Check for timeout waiting for REQ to clear
            if (!req) {
                break;
            }

            // Phase error
            Acquire();
            if (GetPhase() != phase) {
                break;
            }

            buf++;
        }
    }

    EnableIRQ();

    return bytes_sent;
}

bool GPIOBUS::PollSelectEvent()
{
#ifndef USE_SEL_EVENT_ENABLE
    return false;
#else
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

    // Wait for up to 3 s
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
