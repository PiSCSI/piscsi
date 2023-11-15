//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "scsisend/scsisend_core.h"
#include "hal/gpiobus_factory.h"
#include "hal/systimer.h"
#include "controllers/controller_manager.h"
#include "shared/piscsi_exceptions.h"
#include "shared/piscsi_util.h"
#include "generated/piscsi_interface.pb.h"
#include <google/protobuf/util/json_util.h>
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
using namespace piscsi_interface;

void ScsiDump::CleanUp()
{
    if (bus != nullptr) {
        bus->Cleanup();
    }
}

void ScsiDump::TerminationHandler(int)
{
    CleanUp();

	// Process will terminate automatically
}

bool ScsiDump::Banner(span<char *> args) const
{
    cout << piscsi_util::Banner("(PiSCSI Action Trigger Utility)");

    if (args.size() < 2 || string(args[1]) == "-h" || string(args[1]) == "--help") {
        cout << "Usage: " << args[0] << " -t ID[:LUN] [-i BID] -f FILE [-v] [-r] [-s BUFFER_SIZE] [-p] [-I] [-S]\n"
             << " ID is the target device ID (0-" << (ControllerManager::GetScsiIdMax() - 1) << ").\n"
             << " LUN is the optional target device LUN (0-" << (ControllerManager::GetScsiLunMax() -1 ) << ")."
			 << " Default is 0.\n"
             << " BID is the PiSCSI board ID (0-7). Default is 7.\n"
             << " FILE is the dump file path.\n"
             << " BUFFER_SIZE is the transfer buffer size in bytes, at least " << MINIMUM_BUFFER_SIZE
             << " bytes. Default is 1 MiB.\n"
             << " -v Enable verbose logging.\n"
             << " -r Restore instead of dump.\n"
             << " -p Generate .properties file to be used with the PiSCSI web interface. Only valid for dump mode.\n"
			 << " -I Display INQUIRY data of ID[:LUN].\n"
			 << " -S Scan SCSI bus for devices.\n"
             << flush;

        return false;
    }

    return true;
}

bool ScsiDump::Init() const
{
	// Signal handler for cleaning up
	struct sigaction termination_handler;
	termination_handler.sa_handler = TerminationHandler;
	sigemptyset(&termination_handler.sa_mask);
	termination_handler.sa_flags = 0;
	sigaction(SIGTERM, &termination_handler, nullptr);
	signal(SIGPIPE, SIG_IGN);

    bus = GPIOBUS_Factory::Create(BUS::mode_e::INITIATOR);

    return bus != nullptr;
}

void ScsiDump::ParseArguments(span<char *> args)
{
    int opt;

    int buffer_size = DEFAULT_BUFFER_SIZE;

    opterr = 0;
    while ((opt = getopt(static_cast<int>(args.size()), args.data(), "i:f:s:t:rvpIS")) != -1) {
        switch (opt) {
        case 'i':
            if (!GetAsUnsignedInt(optarg, initiator_id) || initiator_id > 7) {
                throw parser_exception("Invalid PiSCSI board ID " + to_string(initiator_id) + " (0-7)");
            }
            break;

        case 'f':
            filename = optarg;
            break;

        case 'I':
        	inquiry = true;
        	break;

        case 's':
            if (!GetAsUnsignedInt(optarg, buffer_size) || buffer_size < MINIMUM_BUFFER_SIZE) {
                throw parser_exception("Buffer size must be at least " + to_string(MINIMUM_BUFFER_SIZE / 1024) + " KiB");
            }

            break;

        case 'S':
        	scan_bus = true;
        	break;

        case 't':
            if (const string error = ProcessId(optarg, target_id, target_lun); !error.empty()) {
                throw parser_exception(error);
            }
            break;

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

    if (!scan_bus && !inquiry && filename.empty()) {
        throw parser_exception("Missing filename");
    }

    if (!scan_bus && target_id == -1) {
    	throw parser_exception("Missing target ID");
    }

    if (target_id == initiator_id) {
        throw parser_exception("Target ID and PiSCSI board ID must not be identical");
    }

    if (target_lun == -1) {
    	target_lun = 0;
    }

    if (scan_bus) {
    	inquiry = false;
    }

    buffer = vector<uint8_t>(buffer_size);
}

void ScsiDump::WaitForPhase(phase_t phase) const
{
    spdlog::debug(string("Waiting for ") + BUS::GetPhaseStrRaw(phase) + " phase");

    // Timeout (3000ms)
    const uint32_t now = SysTimer::GetTimerLow();
    while ((SysTimer::GetTimerLow() - now) < 3'000'000) {
        bus->Acquire();
        if (bus->GetREQ() && bus->GetPhase() == phase) {
            return;
        }
    }

    throw phase_exception("Expected " + string(BUS::GetPhaseStrRaw(phase)) + " phase, actual phase is " +
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
    spdlog::debug("Executing " + command_mapping.find(cmd)->second.second);

    Selection();

    WaitForPhase(phase_t::command);

    cdb[0] = static_cast<uint8_t>(cmd);
    cdb[1] = static_cast<uint8_t>(static_cast<byte>(cdb[1]) | static_cast<byte>(target_lun << 5));
    if (static_cast<int>(cdb.size()) !=
        bus->SendHandShake(cdb.data(), static_cast<int>(cdb.size()), BUS::SEND_NO_DELAY)) {
        BusFree();

        throw phase_exception(command_mapping.find(cmd)->second.second + string(" failed"));
    }
}

int ScsiDump::DataIn(int length)
{
    WaitForPhase(phase_t::datain);

    const int received = bus->ReceiveHandShake(buffer.data(), length);
    if (!received) {
        throw phase_exception("DATA IN failed");
    }

    return received;
}

void ScsiDump::DataOut(int length)
{
    WaitForPhase(phase_t::dataout);

    if (!bus->SendHandShake(buffer.data(), length, BUS::SEND_NO_DELAY)) {
        throw phase_exception("DATA OUT failed");
    }
}

void ScsiDump::Status() const
{
    WaitForPhase(phase_t::status);

    if (array<uint8_t, 256> buf; bus->ReceiveHandShake(buf.data(), 1) != 1) {
        throw phase_exception("STATUS failed");
    }
}

void ScsiDump::MessageIn() const
{
    WaitForPhase(phase_t::msgin);

    if (array<uint8_t, 256> buf; bus->ReceiveHandShake(buf.data(), 1) != 1) {
        throw phase_exception("MESSAGE IN failed");
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

void ScsiDump::Execute()
{
    const string file = restore ? "test.bin" : "test.json";

    // restore means binary
    int size = 0;
    if (!restore) {
        ifstream in(file);
        assert(!in.fail());
        stringstream buf;
        buf << in.rdbuf();
        const string json = buf.str();
        memcpy(buffer.data(), json.data(), json.size());
        size = json.size();
    }
    else {
        ifstream in(file, ios::binary);
        assert(!in.fail());
        vector<char> b(file_size(file));
        in.read(b.data(), b.size());
        memcpy(buffer.data(), b.data(), b.size());
        size = b.size();
    }

    vector<uint8_t> cdb(10);
    cdb[1] = restore ? 0x0a : 0x05;
    cdb[5] = static_cast<uint8_t>(size >> 8);
    cdb[6] = static_cast<uint8_t>(size);
    cdb[7] = static_cast<uint8_t>(65535 >> 8);
    cdb[8] = static_cast<uint8_t>(65535);
    Command(scsi_command::eCmdExecute, cdb);

    DataOut(size);

    const int length = DataIn(65535);

    if (!restore) {
        const string json((const char *)buffer.data(), length);
        cerr << "json received:\n" << json << endl;
    }
    else {
        PbResult result;
        if (!result.ParseFromArray(buffer.data(), length)) {
            assert(false);
        }
        string json;
        google::protobuf::util::MessageToJsonString(result, &json);
        cerr << "json (converted from binary) received:\n" << json << endl;
   }

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
    	throw phase_exception("SELECTION failed");
    }
}

int ScsiDump::run(span<char *> args)
{
    if (!Banner(args)) {
        return EXIT_SUCCESS;
    }

    try {
        ParseArguments(args);
    }
    catch (const parser_exception& e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }

    if (getuid()) {
    	cerr << "Error: GPIO bus access requires root permissions. Are you running as root?" << endl;
        return EXIT_FAILURE;
    }

#ifndef USE_SEL_EVENT_ENABLE
    cerr << "Error: No PiSCSI hardware support" << endl;
    return EXIT_FAILURE;
#endif

    if (!Init()) {
		cerr << "Error: Can't initialize bus" << endl;
        return EXIT_FAILURE;
    }

    try {
        DisplayBoardId();

        inquiry_info_t inq_info;
        DisplayInquiry(inq_info, false);
    }
    catch (const phase_exception& e) {
    	cerr << "Error: " << e.what() << endl;

    	CleanUp();

    	return EXIT_FAILURE;
    }

    CleanUp();

    return EXIT_SUCCESS;
}

void ScsiDump::DisplayBoardId() const
{
    cout << DIVIDER << "\nPiSCSI board ID is " << initiator_id << "\n";
}

void ScsiDump::ScanBus()
{
    DisplayBoardId();

	for (target_id = 0; target_id < ControllerManager::GetScsiIdMax(); target_id++) {
		if (initiator_id == target_id) {
			continue;
		}

		for (target_lun = 0; target_lun < 8; target_lun++) {
			inquiry_info_t inq_info;
			try {
				DisplayInquiry(inq_info, false);
			}
			catch(const phase_exception&) {
				// Continue with next ID if there is no LUN 0
				if (!target_lun) {
					break;
				}
			}
		}
	}
}

bool ScsiDump::DisplayInquiry(ScsiDump::inquiry_info_t& inq_info, bool)
{
    // Assert RST for 1 ms
    bus->SetRST(true);
    const timespec ts = {.tv_sec = 0, .tv_nsec = 1000 * 1000};
    nanosleep(&ts, nullptr);
    bus->SetRST(false);

    cout << DIVIDER << "\nTarget device is " << target_id << ":" << target_lun << "\n" << flush;

    Inquiry();

    const auto type = static_cast<byte>(buffer[0]);
    if ((type & byte{0x1f}) == byte{0x1f}) {
    	// Requested LUN is not available
    	return false;
    }

    array<char, 17> str = {};
    memcpy(str.data(), &buffer[8], 8);
    inq_info.vendor = string(str.data());
    cout << "Vendor:      " << inq_info.vendor << "\n";

    str.fill(0);
    memcpy(str.data(), &buffer[16], 16);
    inq_info.product = string(str.data());
    cout << "Product:     " << inq_info.product << "\n";

    str.fill(0);
    memcpy(str.data(), &buffer[32], 4);
    inq_info.revision = string(str.data());
    cout << "Revision:    " << inq_info.revision << "\n" << flush;

    if (const auto& t = DEVICE_TYPES.find(type & byte{0x1f}); t != DEVICE_TYPES.end()) {
    	cout << "Device Type: " << (*t).second << "\n";
    }
    else {
    	cout << "Device Type: Unknown\n";
    }

    cout << "Removable:   " << (((static_cast<byte>(buffer[1]) & byte{0x80}) == byte{0x80}) ? "Yes" : "No") << "\n";

    Execute();

    return true;
}


bool ScsiDump::GetDeviceInfo(inquiry_info_t& inq_info)
{
    DisplayBoardId();

    if (!DisplayInquiry(inq_info, true)) {
    	return false;
    }

    TestUnitReady();

    RequestSense();

    return true;
}

void ScsiDump::inquiry_info::GeneratePropertiesFile(const string& property_file) const
{
	ofstream prop_stream(property_file);

    prop_stream << "{" << endl;
    prop_stream << "   \"vendor\": \"" << vendor << "\"," << endl;
    prop_stream << "   \"product\": \"" << product << "\"," << endl;
    prop_stream << "   \"revision\": \"" << revision << "\"," << endl;
    prop_stream << "   \"block_size\": \"" << sector_size << "\"" << endl;
    prop_stream << "}" << endl;

    if (prop_stream.fail()) {
        spdlog::warn("Unable to create properties file '" + property_file + "'");
    }
}
