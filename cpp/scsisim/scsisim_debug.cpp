//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI for Raspberry Pi
//
//	Copyright (C) 2023 akuker
//
//	[ SCSI Bus Emulator ]
//
//---------------------------------------------------------------------------
#include "hal/gpiobus_factory.h"
#include "hal/gpiobus_virtual.h"
#include "scsisim_core.h"
#include "shared/log.h"
#include "shared/piscsi_util.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include "hal/data_sample.h"
#include "scsisim_defs.h"
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <sys/mman.h>

void ScsiSim::PrintDifferences(const DataSample& current, const DataSample& previous)
{
    if (current.GetRawCapture() != previous.GetRawCapture()) {
        stringstream s;
        s << "Data changed: <";
        s << string(current.GetPhaseStr()) << "> ";

        if (current.GetDAT() != previous.GetDAT()) {
            s << "DAT(" << setw(2) << hex << (int)current.GetDAT() << ") ";
        }
        if (current.GetBSY() != previous.GetBSY()) {
            s << "BSY(" << current.GetBSY() << ") ";
        }
        if (current.GetSEL() != previous.GetSEL()) {
            s << "SEL(" << current.GetSEL() << ") ";
        }
        if (current.GetATN() != previous.GetATN()) {
            s << "ATN(" << current.GetATN() << ") ";
        }
        if (current.GetACK() != previous.GetACK()) {
            s << "ACK(" << current.GetACK() << ") ";
        }
        if (current.GetRST() != previous.GetRST()) {
            s << "RST(" << current.GetRST() << ") ";
        }
        if (current.GetMSG() != previous.GetMSG()) {
            s << "MSG(" << current.GetMSG() << ") ";
        };
        if (current.GetCD() != previous.GetCD()) {
            s << "CD(" << current.GetCD() << ") ";
        }
        if (current.GetIO() != previous.GetIO()) {
            s << "IO(" << current.GetIO() << ") ";
        }
        if (current.GetREQ() != previous.GetREQ()) {
            s << "REQ(" << current.GetREQ() << ") ";
        }
        if (current.GetACT() != previous.GetACT()) {
            s << "ACT(" << current.GetACT() << ") ";
        }
        s << flush;

        LOGTRACE("%s", s.str().c_str());
    }
}

void ScsiSim::TestClient()
{
    LOGINFO("TESTING!!!")
    int sleep_time = 10000;

    unique_ptr<BUS> bus = GPIOBUS_Factory::Create(BUS::mode_e::TARGET);

    if (!bus->SharedMemValid()) {
        LOGWARN("Unable to set up shared memory region");
        return;
    }

    LOGINFO("bus->SetBSY");
    bus->SetBSY(1);
    usleep(sleep_time);
    bus->SetBSY(0);
    usleep(sleep_time);
    LOGINFO("bus->SetSEL");
    bus->SetSEL(1);
    usleep(sleep_time);
    bus->SetSEL(0);
    usleep(sleep_time);
    LOGINFO("bus->SetATN");
    bus->SetATN(1);
    usleep(sleep_time);
    bus->SetATN(0);
    usleep(sleep_time);
    LOGINFO("bus->SetACK");
    bus->SetACK(1);
    usleep(sleep_time);
    bus->SetACK(0);
    usleep(sleep_time);
    LOGINFO("bus->SetRST");
    bus->SetRST(1);
    usleep(sleep_time);
    bus->SetRST(0);
    usleep(sleep_time);
    LOGINFO("bus->SetMSG");
    bus->SetMSG(1);
    usleep(sleep_time);
    bus->SetMSG(0);
    usleep(sleep_time);
    LOGINFO("bus->SetCD");
    bus->SetCD(1);
    usleep(sleep_time);
    bus->SetCD(0);
    usleep(sleep_time);
    LOGINFO("bus->SetIO");
    bus->SetIO(1);
    usleep(sleep_time);
    bus->SetIO(0);
    usleep(sleep_time);
    LOGINFO("bus->SetREQ");
    bus->SetREQ(1);
    usleep(sleep_time);
    bus->SetREQ(0);
    usleep(sleep_time);
    LOGINFO("bus->SetACT");
    bus->SetACT(1);
    usleep(sleep_time);
    bus->SetACT(0);
    usleep(sleep_time);
    for (uint32_t i = 0; i <= 0xFF; i++) {
        LOGINFO("bus->SetDAT(%02X)", i);
        bus->SetDAT((uint8_t)i);
        usleep(sleep_time);
        bus->SetDAT(0);
        usleep(sleep_time);
    }

    if (bus != nullptr) {
        LOGTRACE("%s trying bus cleanup", __PRETTY_FUNCTION__)
        bus->Cleanup();
    }
}