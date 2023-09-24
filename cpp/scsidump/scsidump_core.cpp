//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2022 akuker
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

// TODO Evaluate CHECK CONDITION after sending a command
// TODO Send IDENTIFY message in order to support LUNS > 7

#include "scsidump/scsidump_core.h"
#include "hal/gpiobus.h"
#include "hal/gpiobus_factory.h"
#include "hal/systimer.h"
#include "controllers/controller_manager.h"
#include "shared/piscsi_exceptions.h"
#include "shared/piscsi_util.h"
#include "shared/piscsi_version.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>

using namespace std;
using namespace filesystem;
using namespace spdlog;
using namespace scsi_defs;
using namespace piscsi_util;

void ScsiDump::CleanUp()
{
    if (bus != nullptr) {
        bus->Cleanup();
    }
}

void ScsiDump::KillHandler(int)
{
    CleanUp();

    exit(EXIT_SUCCESS);
}

bool ScsiDump::Banner(span<char *> args) const
{
    cout << piscsi_util::Banner("(Hard Disk Dump/Restore Utility)");

    if (args.size() < 2 || string(args[1]) == "-h" || string(args[1]) == "--help") {
        cout << "Usage: " << args[0] << " -t ID[:LUN] [-i BID] -f FILE [-v] [-r] [-s BUFFER_SIZE] [-p]\n"
             << " ID is the target device ID (0-" << (ControllerManager::GetScsiIdMax() - 1) << ").\n"
             << " LUN is the optional target device LUN (0-" << (ControllerManager::GetScsiLunMax() -1 ) << ")."
			 << " Default is 0.\n"
             << " BID is the PiSCSI board ID (0-7). Default is 7.\n"
             << " FILE is the dump file path.\n"
             << " BUFFER_SIZE is the transfer buffer size in bytes, at least " << to_string(MINIMUM_BUFFER_SIZE)
             << " bytes. Default is 1 MiB.\n"
             << " -v Enable verbose logging.\n"
             << " -r Restore instead of dump.\n"
             << " -p Generate .properties file to be used with the PiSCSI web interface. Only valid for dump mode.\n"
             << flush;

        return false;
    }

    return true;
}

bool ScsiDump::Init() const
{
    // Interrupt handler setting
    if (signal(SIGINT, KillHandler) == SIG_ERR || signal(SIGHUP, KillHandler) == SIG_ERR ||
        signal(SIGTERM, KillHandler) == SIG_ERR) {
        return false;
    }

    bus = GPIOBUS_Factory::Create(BUS::mode_e::INITIATOR);

    return bus != nullptr;
}

void ScsiDump::ParseArguments(span<char *> args)
{
    int opt;

    int buffer_size = DEFAULT_BUFFER_SIZE;

    opterr = 0;
    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "i:f:s:t:rvp")) != -1) {
        switch (opt) {
        case 'i':
            if (!GetAsUnsignedInt(optarg, initiator_id) || initiator_id > 7) {
                throw parser_exception("Invalid PiSCSI board ID " + to_string(initiator_id) + " (0-7)");
            }
            break;

        case 'f':
            filename = optarg;
            break;

        case 's':
            if (!GetAsUnsignedInt(optarg, buffer_size) || buffer_size < MINIMUM_BUFFER_SIZE) {
                throw parser_exception("Buffer size must be at least " + to_string(MINIMUM_BUFFER_SIZE / 1024) + "KiB");
            }

            break;

        case 't': {
            const string error = ProcessId(optarg, target_id, target_lun);
            if (!error.empty()) {
                throw parser_exception(error);
            }
        } break;

        case 'v':
            set_level(level::debug);
            break;

        case 'r':
            restore = true;
            break;

        case 'p':
            properties_file = true;
            break;

        default:
            break;
        }
    }

    if (target_id == initiator_id) {
        throw parser_exception("Target ID and PiSCSI board ID must not be identical");
    }

    if (filename.empty()) {
        throw parser_exception("Missing filename");
    }

    buffer = vector<uint8_t>(buffer_size);
}

void ScsiDump::WaitPhase(phase_t phase) const
{
    spdlog::debug(string("Waiting for ") + BUS::GetPhaseStrRaw(phase) + " phase");

    // Timeout (3000ms)
    const uint32_t now = SysTimer::GetTimerLow();
    while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000) {
        bus->Acquire();
        if (bus->GetREQ() && bus->GetPhase() == phase) {
            return;
        }
    }

    throw parser_exception("Expected " + string(BUS::GetPhaseStrRaw(phase)) + " phase, actual phase is " +
                           string(BUS::GetPhaseStrRaw(bus->GetPhase())));
}

void ScsiDump::Selection() const
{
    // Set initiator and target ID
    auto data = static_cast<byte>(1 << initiator_id);
    data |= static_cast<byte>(1 << target_id);
    bus->SetDAT(static_cast<uint8_t>(data));

    bus->SetSEL(true);

    WaitForBusy();

    bus->SetSEL(false);
}

void ScsiDump::Command(scsi_command cmd, vector<uint8_t>& cdb) const
{
    spdlog::debug("Executing " + string(command_mapping.find(cmd)->second.second));

    Selection();

    WaitPhase(phase_t::command);

    cdb[0] = static_cast<uint8_t>(cmd);
    cdb[1] = static_cast<uint8_t>(static_cast<byte>(cdb[1]) | static_cast<byte>(target_lun << 5));
    if (static_cast<int>(cdb.size()) !=
        bus->SendHandShake(cdb.data(), static_cast<int>(cdb.size()), BUS::SEND_NO_DELAY)) {
        BusFree();

        throw parser_exception(command_mapping.find(cmd)->second.second + string(" failed"));
    }
}

void ScsiDump::DataIn(int length)
{
    WaitPhase(phase_t::datain);

    if (!bus->ReceiveHandShake(buffer.data(), length)) {
        throw parser_exception("DATA IN failed");
    }
}

void ScsiDump::DataOut(int length)
{
    WaitPhase(phase_t::dataout);

    if (!bus->SendHandShake(buffer.data(), length, BUS::SEND_NO_DELAY)) {
        throw parser_exception("DATA OUT failed");
    }
}

void ScsiDump::Status() const
{
    WaitPhase(phase_t::status);

    if (array<uint8_t, 256> buf; bus->ReceiveHandShake(buf.data(), 1) != 1) {
        throw parser_exception("STATUS failed");
    }
}

void ScsiDump::MessageIn() const
{
    WaitPhase(phase_t::msgin);

    if (array<uint8_t, 256> buf; bus->ReceiveHandShake(buf.data(), 1) != 1) {
        throw parser_exception("MESSAGE IN failed");
    }
}

void ScsiDump::BusFree() const
{
    bus->Reset();
}

void ScsiDump::TestUnitReady() const
{
    vector<uint8_t> cdb(6);
    Command(scsi_command::eCmdTestUnitReady, cdb);

    Status();

    MessageIn();

    BusFree();
}

void ScsiDump::RequestSense()
{
    vector<uint8_t> cdb(6);
    cdb[4] = 0xff;
    Command(scsi_command::eCmdRequestSense, cdb);

    DataIn(256);

    Status();

    MessageIn();

    BusFree();
}

void ScsiDump::Inquiry()
{
    vector<uint8_t> cdb(6);
    cdb[4] = 0xff;
    Command(scsi_command::eCmdInquiry, cdb);

    DataIn(256);

    Status();

    MessageIn();

    BusFree();
}

pair<uint64_t, uint32_t> ScsiDump::ReadCapacity()
{
    vector<uint8_t> cdb(10);
    Command(scsi_command::eCmdReadCapacity10, cdb);

    DataIn(8);

    Status();

    MessageIn();

    BusFree();

    uint64_t capacity = (static_cast<uint32_t>(buffer[0]) << 24) | (static_cast<uint32_t>(buffer[1]) << 16) |
                        (static_cast<uint32_t>(buffer[2]) << 8) | static_cast<uint32_t>(buffer[3]);

    int sector_size_offset = 4;

    if (static_cast<int32_t>(capacity) == -1) {
        cdb.resize(16);
        // READ CAPACITY(16), not READ LONG(16)
        cdb[1] = 0x10;
        Command(scsi_command::eCmdReadCapacity16_ReadLong16, cdb);

        DataIn(14);

        Status();

        MessageIn();

        BusFree();

        capacity = (static_cast<uint64_t>(buffer[0]) << 56) | (static_cast<uint64_t>(buffer[1]) << 48) |
                   (static_cast<uint64_t>(buffer[2]) << 40) | (static_cast<uint64_t>(buffer[3]) << 32) |
                   (static_cast<uint64_t>(buffer[4]) << 24) | (static_cast<uint64_t>(buffer[5]) << 16) |
                   (static_cast<uint64_t>(buffer[6]) << 8) | static_cast<uint64_t>(buffer[7]);

        sector_size_offset = 8;
    }

    const uint32_t sector_size = (static_cast<uint32_t>(buffer[sector_size_offset]) << 24) |
                                 (static_cast<uint32_t>(buffer[sector_size_offset + 1]) << 16) |
                                 (static_cast<uint32_t>(buffer[sector_size_offset + 2]) << 8) |
                                 static_cast<uint32_t>(buffer[sector_size_offset + 3]);

    return { capacity, sector_size };
}

void ScsiDump::Read10(uint32_t bstart, uint32_t blength, uint32_t length)
{
    vector<uint8_t> cdb(10);
    cdb[2] = (uint8_t)(bstart >> 24);
    cdb[3] = (uint8_t)(bstart >> 16);
    cdb[4] = (uint8_t)(bstart >> 8);
    cdb[5] = (uint8_t)bstart;
    cdb[7] = (uint8_t)(blength >> 8);
    cdb[8] = (uint8_t)blength;
    Command(scsi_command::eCmdRead10, cdb);

    DataIn(length);

    Status();

    MessageIn();

    BusFree();
}

void ScsiDump::Write10(uint32_t bstart, uint32_t blength, uint32_t length)
{
    vector<uint8_t> cdb(10);
    cdb[2] = (uint8_t)(bstart >> 24);
    cdb[3] = (uint8_t)(bstart >> 16);
    cdb[4] = (uint8_t)(bstart >> 8);
    cdb[5] = (uint8_t)bstart;
    cdb[7] = (uint8_t)(blength >> 8);
    cdb[8] = (uint8_t)blength;
    Command(scsi_command::eCmdWrite10, cdb);

    DataOut(length);

    Status();

    MessageIn();

    BusFree();
}

void ScsiDump::WaitForBusy() const
{
    // Wait for busy for up to 2 s
    int count = 10000;
    do {
        // Wait 20 ms
        const timespec ts = {.tv_sec = 0, .tv_nsec = 20 * 1000};
        nanosleep(&ts, nullptr);
        bus->Acquire();
        if (bus->GetBSY()) {
            break;
        }
    } while (count--);

    // Success if the target is busy
    if (!bus->GetBSY()) {
        throw parser_exception("SELECTION failed");
    }
}

int ScsiDump::run(span<char *> args)
{
    if (!Banner(args)) {
        return EXIT_SUCCESS;
    }

    if (!Init()) {
        cerr << "Error: Initializing. Are you root?" << endl;

        // Probably not root
        return EPERM;
    }

    try {
        ParseArguments(args);

#ifndef USE_SEL_EVENT_ENABLE
        cerr << "Error: No PiSCSI hardware support" << endl;
        return EXIT_FAILURE;
#endif

        return DumpRestore();
    } catch (const parser_exception& e) {
        cerr << "Error: " << e.what() << endl;

        CleanUp();

        return EXIT_FAILURE;
    }

    CleanUp();

    return EXIT_SUCCESS;
}

int ScsiDump::DumpRestore()
{
    const auto inq_info = GetDeviceInfo();

    fstream fs;
    fs.open(filename, (restore ? ios::in : ios::out) | ios::binary);

    if (fs.fail()) {
        throw parser_exception("Can't open image file '" + filename + "'");
    }

    if (restore) {
        cout << "Starting restore\n" << flush;

        off_t size;
        try {
        	size = file_size(path(filename));
        }
        catch (const filesystem_error& e) {
        	throw parser_exception(string("Can't determine file size: ") + e.what());
        }

        cout << "Restore file size: " << size << " bytes\n";
        if (size > (off_t)(inq_info.sector_size * inq_info.capacity)) {
            cout << "WARNING: File size is larger than disk size\n" << flush;
        } else if (size < (off_t)(inq_info.sector_size * inq_info.capacity)) {
            throw parser_exception("File size is smaller than disk size");
        }
    } else {
        cout << "Starting dump\n" << flush;
    }

    // Dump by buffer size
    auto dsiz = static_cast<int>(buffer.size());
    const int duni = dsiz / inq_info.sector_size;
    auto dnum = static_cast<int>((inq_info.capacity * inq_info.sector_size) / dsiz);

    auto start_time = chrono::high_resolution_clock::now();

    int i;
    for (i = 0; i < dnum; i++) {
        if (restore) {
            fs.read((char*)buffer.data(), dsiz);
            Write10(i * duni, duni, dsiz);
        } else {
            Read10(i * duni, duni, dsiz);
            fs.write((const char*)buffer.data(), dsiz);
        }

        if (fs.fail()) {
            throw parser_exception("File I/O failed");
        }

        cout << ((i + 1) * 100 / dnum) << "%"
             << " (" << (i + 1) * duni << "/" << inq_info.capacity << ")\n"
             << flush;
    }

    // Rounding on capacity
    dnum = inq_info.capacity % duni;
    dsiz = dnum * inq_info.sector_size;
    if (dnum > 0) {
        if (restore) {
            fs.read((char*)buffer.data(), dsiz);
            if (!fs.fail()) {
                Write10(i * duni, dnum, dsiz);
            }
        } else {
            Read10(i * duni, dnum, dsiz);
            fs.write((const char*)buffer.data(), dsiz);
        }

        if (fs.fail()) {
            throw parser_exception("File I/O failed");
        }

        cout << "100% (" << inq_info.capacity << "/" << inq_info.capacity << ")\n" << flush;
    }

    auto stop_time = chrono::high_resolution_clock::now();

    auto duration = chrono::duration_cast<chrono::seconds>(stop_time - start_time).count();

    cout << divider_str << "\n";
    cout << "Transfered : " << to_string(inq_info.capacity * inq_info.sector_size) << " bytes ["
         << to_string(inq_info.capacity * inq_info.sector_size / 1024 / 1024) << "MiB]\n";
    cout << "Total time: " << to_string(duration) << " seconds (" << to_string(duration / 60) << " minutes\n";
    cout << "Averate transfer rate: " << to_string((inq_info.capacity * inq_info.sector_size / 8) / duration)
         << " bytes per second (" << to_string((inq_info.capacity * inq_info.sector_size / 8) / duration / 1024)
         << " KiB per second)\n";
    cout << divider_str << "\n";

    if (properties_file && !restore) {
        GeneratePropertiesFile(filename, inq_info);
    }

    return EXIT_SUCCESS;
}

ScsiDump::inquiry_info_t ScsiDump::GetDeviceInfo()
{
    // Assert RST for 1 ms
    bus->SetRST(true);
    const timespec ts = {.tv_sec = 0, .tv_nsec = 1000 * 1000};
    nanosleep(&ts, nullptr);
    bus->SetRST(false);

    cout << divider_str << "\n";
    cout << "PiSCSI board ID: " << initiator_id << "\n";
    cout << divider_str << "\n" << flush;
    cout << "Target device ID: " << target_id << ", LUN: " << target_lun << "\n";

    Inquiry();

    inquiry_info_t inq_info;

    // Display INQUIRY information
    array<char, 17> str = {};
    memcpy(str.data(), &buffer[8], 8);
    cout << "Vendor:       " << str.data() << "\n";
    inq_info.vendor = string(str.data());

    str.fill(0);
    memcpy(str.data(), &buffer[16], 16);
    cout << "Product:      " << str.data() << "\n";
    inq_info.product = string(str.data());

    str.fill(0);
    memcpy(str.data(), &buffer[32], 4);
    cout << "Revision:     " << str.data() << "\n" << flush;
    inq_info.revision = string(str.data());

    if (auto type = static_cast<device_type>(buffer[0]);
        type != device_type::direct_access && type != device_type::cd_rom && type != device_type::optical_memory) {
        throw parser_exception("Invalid device type, supported types are DIRECT ACCESS, CD-ROM and OPTICAL MEMORY");
    }

    TestUnitReady();

    RequestSense();

    const auto [capacity, sector_size] = ReadCapacity();
    inq_info.capacity = capacity;
    inq_info.sector_size = sector_size;

    cout << "Sectors:      " << capacity << "\n"
         << "Sector size:  " << sector_size << " bytes\n"
         << "Capacity:     " << sector_size * capacity / 1024 / 1024 << " MiB (" << sector_size * capacity
         << " bytes)\n"
         << divider_str << "\n\n"
         << flush;

    return inq_info;
}

void ScsiDump::GeneratePropertiesFile(const string& filename, const inquiry_info_t& inq_info)
{
    string prop_filename = filename + ".properties";
    stringstream prop_stream;

    prop_stream << "{" << endl;
    prop_stream << "   \"vendor\": \"" << inq_info.vendor << "\"," << endl;
    prop_stream << "   \"product\": \"" << inq_info.product << "\"," << endl;
    prop_stream << "   \"revision\": \"" << inq_info.revision << "\"," << endl;
    prop_stream << "   \"block_size\": \"" << to_string(inq_info.sector_size) << "\"," << endl;
    prop_stream << "}" << endl;

    FILE* fp = fopen(prop_filename.c_str(), "w");
    if (fp) {
        fputs(prop_stream.str().c_str(), fp);
    } else {
        spdlog::warn("Unable to open output file '" + prop_filename + "'");
        return;
    }

    fclose(fp);
}
