//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "hal/gpiobus.h"
#include "hal/systimer.h"
#include "rascsi_version.h"
#include "rasdump/rasdump_core.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <sys/stat.h>
#include <csignal>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fstream>

using namespace std;
using namespace spdlog;
using namespace scsi_defs;

RasDump::~RasDump()
{
	if (bus != nullptr) {
		bus->Cleanup();
	}
}

void RasDump::KillHandler(int)
{
	exit(EXIT_SUCCESS);
}

bool RasDump::Banner(const vector<char *>& args) const
{
	cout << "RaSCSI hard disk dump utility version " << rascsi_get_version_string()
			<< " (" << __DATE__ << ", " << __TIME__ << ")\n" << flush;

	if (args.size() < 2 || string(args[1]) == "-h") {
		cout << "Usage: " << args[0] << " -i ID [-u LUN] [-b BID] -f FILE [-l] [-r] [-s BUFFER_SIZE]\n"
				<< " ID is the target device SCSI ID (0-7).\n"
				<< " LUN is the target device LUN (0-7). Default is 0.\n"
				<< " BID is the RaSCSI board SCSI ID (0-7). Default is 7.\n"
				<< " FILE is the dump file path.\n"
				<< " BUFFER_SIZE is the transfer buffer size, at least 64 KiB. Default is 64 KiB.\n"
				<< " -v Enable verbose logging.\n"
				<< " -r Restore instead of dump.\n" << flush;

		return false;
	}

	return true;
}

bool RasDump::Init()
{
	// Interrupt handler setting
	if (signal(SIGINT, KillHandler) == SIG_ERR || signal(SIGHUP, KillHandler) == SIG_ERR ||
			signal(SIGTERM, KillHandler) == SIG_ERR) {
		return false;
	}

	bus = make_unique<GPIOBUS>();

	return bus->Init(BUS::mode_e::INITIATOR);
}

bool RasDump::ParseArguments(const vector<char *>& args)
{
	int opt;

	int buffer_size = DEFAULT_BUFFER_SIZE;

	opterr = 0;
	while ((opt = getopt(static_cast<int>(args.size()), args.data(), "i:b:f:s:u:rv")) != -1) {
		switch (tolower(opt)) {
			case 'i':
				if (!GetAsInt(optarg, target_id) || target_id > 7) {
					cerr << "Error: Invalid target ID " << target_id << endl;
					return false;
				}
				break;

			case 'b':
				if (!GetAsInt(optarg, initiator_id) || initiator_id > 7) {
					cerr << "Error: Invalid RaSCSI ID " << initiator_id << endl;
					return false;
				}
				break;

			case 'f':
				filename = optarg;
				break;

			case 's':
				if (!GetAsInt(optarg, buffer_size) || buffer_size < DEFAULT_BUFFER_SIZE) {
					cerr << "Error: Buffer size must be at least 64 KiB";
				}

				break;

			case 'u':
				if (!GetAsInt(optarg, target_lun) || target_lun > 7) {
					cerr << "Error: Invalid target LUN " << initiator_id << endl;
					return false;
				}
				break;

			case 'v':
				set_level(level::debug);
				break;

			case 'r':
				restore = true;
				break;

			default:
				break;
		}
	}

	if (target_id == initiator_id) {
		cerr << "Error: Target ID and RaSCSI ID must not be identical" << endl;
		return false;
	}

	if (filename.empty()) {
		cerr << "Error: Missing filename" << endl;
		return false;
	}

	buffer = vector<uint8_t>(buffer_size);

	return true;
}

void RasDump::WaitPhase(BUS::phase_t phase) const
{
	// Timeout (3000ms)
	const uint32_t now = SysTimer::GetTimerLow();
	while ((SysTimer::GetTimerLow() - now) < 3 * 1000 * 1000) {
		bus->Acquire();
		if (bus->GetREQ() && bus->GetPhase() == phase) {
			return;
		}
	}

	throw rasdump_exception("Expected bus phase " + string(BUS::GetPhaseStrRaw(phase)) + ", actual phase is "
			+ string(BUS::GetPhaseStrRaw(bus->GetPhase())));
}

void RasDump::Selection() const
{
	// Set initiator and target ID
	auto data = static_cast<uint8_t>(1 << initiator_id);
	data |= (1 << target_id);
	bus->SetDAT(data);

	bus->SetSEL(true);

	// Wait for busy for up to 2 s
	int count = 10000;
	do {
		// Wait 20 ms
		const timespec ts = { .tv_sec = 0, .tv_nsec = 20 * 1000};
		nanosleep(&ts, nullptr);
		bus->Acquire();
		if (bus->GetBSY()) {
			break;
		}
	} while (count--);

	bus->SetSEL(false);

	// Success if the target is busy
	if(!bus->GetBSY()) {
		throw rasdump_exception("SELECTION failed");
	}
}

void RasDump::Command(scsi_command cmd, vector<uint8_t>& cdb) const
{
	LOGDEBUG("Executing %s", command_mapping.find(cmd)->second.second)

	Selection();

	// TODO Send identify message for LUN selection
	//bus->SetATN(true);
	//buffer[0] = 0x80 | target_lun;
	//MessageOut()

	WaitPhase(BUS::phase_t::command);

	// Send command. Success if the transmission result is the same as the number of requests
	cdb[0] = static_cast<uint8_t>(cmd);
	cdb[1] |= target_lun << 5;
	if (static_cast<int>(cdb.size()) != bus->SendHandShake(cdb.data(), static_cast<int>(cdb.size()), BUS::SEND_NO_DELAY)) {
		BusFree();

		throw rasdump_exception(command_mapping.find(cmd)->second.second + string(" failed"));
	}
}

void RasDump::DataIn(int length)
{
	WaitPhase(BUS::phase_t::datain);

	if (!bus->ReceiveHandShake(buffer.data(), length)) {
		throw rasdump_exception("DATA IN failed");
	}
}

void RasDump::DataOut(int length)
{
	WaitPhase(BUS::phase_t::dataout);

	if (!bus->SendHandShake(buffer.data(), length, BUS::SEND_NO_DELAY)) {
		throw rasdump_exception("DATA OUT failed");
	}
}

void RasDump::Status() const
{
	WaitPhase(BUS::phase_t::status);

	if (array<uint8_t, 256> buf; bus->ReceiveHandShake(buf.data(), 1) != 1) {
		throw rasdump_exception("STATUS failed");
	}
}

void RasDump::MessageIn() const
{
	WaitPhase(BUS::phase_t::msgin);

	if (array<uint8_t, 256> buf; bus->ReceiveHandShake(buf.data(), 1) != 1) {
		throw rasdump_exception("MESSAGE IN failed");
	}
}

void RasDump::MessageOut()
{
	WaitPhase(BUS::phase_t::msgout);

	if (!bus->SendHandShake(buffer.data(), 1, BUS::SEND_NO_DELAY)) {
		throw rasdump_exception("MESSAGE OUT failed");
	}
}

void RasDump::BusFree() const
{
	bus->Reset();
}

void RasDump::TestUnitReady() const
{
	vector<uint8_t> cdb(6);
	Command(scsi_command::eCmdTestUnitReady, cdb);

	Status();

	MessageIn();

	BusFree();
}

void RasDump::RequestSense()
{
	vector<uint8_t> cdb(6);
	cdb[4] = 0xff;
	Command(scsi_command::eCmdRequestSense, cdb);

	DataIn(256);

	Status();

	MessageIn();

	BusFree();
}

void RasDump::Inquiry()
{
	vector<uint8_t> cdb(6);
	cdb[4] = 0xff;
	Command(scsi_command::eCmdInquiry, cdb);

	DataIn(256);

	Status();

	MessageIn();

	BusFree();
}

void RasDump::ReadCapacity()
{
	vector<uint8_t> cdb(10);
	Command(scsi_command::eCmdReadCapacity10, cdb);

	DataIn(8);

	Status();

	MessageIn();

	BusFree();
}

void RasDump::Read10(uint32_t bstart, uint32_t blength, uint32_t length)
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

void RasDump::Write10(uint32_t bstart, uint32_t blength, uint32_t length)
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

int RasDump::run(const vector<char *>& args)
{
	if (!Banner(args)) {
		return EXIT_SUCCESS;
	}

	if (!Init()) {
		cerr << "Error: Initializing. Are you root?" << endl;

		// Probably not root
		return EPERM;
	}

	if (!ParseArguments(args)) {
		return EINVAL;
	}

#ifndef USE_SEL_EVENT_ENABLE
	cerr << "Error: No RaSCSI hardware support" << endl;
	return EXIT_FAILURE;
#endif

	bus->Reset();

	try {
		return DumpRestore();
	}
	catch(const rasdump_exception& e) {
		cerr << "Error: " << e.what() << endl;

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

int RasDump::DumpRestore()
{
	fstream fs;
	fs.open(filename, (restore ? ios::in : ios::out) | ios::binary);

	if (fs.fail()) {
		throw rasdump_exception("Can't open image file '" + filename + "'");
	}

	bus->Reset();

	// Assert RST for 1 ms
	bus->SetRST(true);
	const timespec ts = { .tv_sec = 0, .tv_nsec = 1000 * 1000};
	nanosleep(&ts, nullptr);
	bus->SetRST(false);

	cout << "Target ID: " << target_id << "\n";
	cout << "RaSCSI ID: " << initiator_id << "\n" << flush;

	Inquiry();

	// Display INQUIRY information
	array<char, 17> str = {};
	memcpy(str.data(), &buffer[8], 8);
	cout << "Vendor:    " << str.data() << "\n";
	str.fill(0);
	memcpy(str.data(), &buffer[16], 16);
	cout << "Product:   " << str.data() << "\n";
	str.fill(0);
	memcpy(str.data(), &buffer[32], 4);
	cout << "Revision:  " << str.data() << "\n" << flush;

	const device_type type = static_cast<device_type>(buffer[0]);
	if (type != device_type::DIRECT_ACCESS && type != device_type::CD_ROM && type != device_type::OPTICAL_MEMORY) {
		throw rasdump_exception("Invalid device type, supported types are DIRECT ACCESS, CD-ROM and OPTICAL MEMORY");
	}

	TestUnitReady();

	RequestSense();

	ReadCapacity();

	const int sector_size = (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
	const int capacity = ((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]) + 1;

	cout << "Number of sectors: " << capacity << "\n"
			<< "Sector size:       " << sector_size << " bytes\n"
			<< "Capacity:          " << sector_size * capacity / 1024 / 1024 << " MiB ("
			<< sector_size * capacity << " bytes)\n" << flush;

	if (restore) {
		off_t size;

		// filesystem::file_size cannot be used here because gcc < 10.3.0 cannot handle more than 2 GiB
		if (struct stat st; !stat(filename.c_str(), &st)) {
			size = st.st_size;
		}
		else {
			throw rasdump_exception("Can't determine file size");
		}

		cout << "Restore file size: " << size << " bytes";
		if (size > (off_t)(sector_size * capacity)) {
			cout << "WARNING: File size is larger than disk size\n" << flush;
		} else if (size < (off_t)(sector_size * capacity)) {
			throw rasdump_exception("File size is smaller than disk size");
		}
	}

	// Dump by buffer size
	int dsiz = static_cast<int>(buffer.size());
	const int duni = dsiz / sector_size;
	int dnum = (capacity * sector_size) / dsiz;

	if (restore) {
		cout <<"Restore progress: " << flush;
	} else {
		cout << "Dump progress: " << flush;
	}

	int i;
	for (i = 0; i < dnum; i++) {
		if (i > 0) {
			printf("\033[21D");
			printf("\033[0K");
		}
		printf("%3d%% (%7d/%7d)", ((i + 1) * 100 / dnum), i * duni, capacity);
		fflush(stdout);

		if (restore) {
			fs.read((char *)buffer.data(), dsiz);
			Write10(i * duni, duni, dsiz);
		}
		else {
			Read10(i * duni, duni, dsiz);
			fs.write((const char *)buffer.data(), dsiz);
		}

		if (fs.fail()) {
			throw rasdump_exception("File I/O failed");
		}
	}

	if (dnum > 0) {
		printf("\033[21D");
		printf("\033[0K");
	}

	// Rounding on capacity
	dnum = capacity % duni;
	dsiz = dnum * sector_size;
	if (dnum > 0) {
		if (restore) {
			fs.read((char *)buffer.data(), dsiz);
			if (!fs.fail()) {
				Write10(i * duni, dnum, dsiz);
			}
		}
		else {
			Read10(i * duni, dnum, dsiz);
			fs.write((const char *)buffer.data(), dsiz);
		}

		if (fs.fail()) {
			throw rasdump_exception("File I/O failed");
		}
	}

	// Completion Message
	printf("%3d%%(%7d/%7d)\n", 100, capacity, capacity);

	return EXIT_SUCCESS;
}

bool RasDump::GetAsInt(const string& value, int& result)
{
	if (value.find_first_not_of("0123456789") != string::npos) {
		return false;
	}

	try {
		auto v = stoul(value);
		result = (int)v;
	}
	catch(const invalid_argument&) {
		return false;
	}
	catch(const out_of_range&) {
		return false;
	}

	return true;
}
