//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi.h"
#include "hal/bus.h"
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

using namespace std;

class rasdump_exception : public runtime_error
{
	using runtime_error::runtime_error;
};

class RasDump //NOSONAR Destructor is required for resource management
{
	// 64 KB buffer
	// TODO Make buffer size configurable
	static const int BUFSIZE = 1024 * 64;

public:

	RasDump() = default;
	~RasDump();

	int run(const vector<char *>&);

private:

	bool Banner(const vector<char *>&) const;
	bool Init();
	bool ParseArguments(const vector<char *>&);
	int DumpRestore();
	void WaitPhase(BUS::phase_t) const;
	void Selection() const;
	void Command(scsi_defs::scsi_command, vector<uint8_t>&) const;
	void DataIn(int);
	void DataOut(int);
	void Status() const;
	void MessageIn() const;
	void TestUnitReady() const;
	void RequestSense();
	void Inquiry();
	void ReadCapacity();
	void Read10(uint32_t, uint32_t, uint32_t);
	void Write10(uint32_t, uint32_t, uint32_t);

	// TODO Use ras_util after removing its dependencies on protobuf interface
	static bool GetAsInt(const string&, int&);
	// TODO Share this code
	bool SetLogLevel(const string&) const;

	static void KillHandler(int);

	unique_ptr<BUS> bus;

	vector<uint8_t> buffer = vector<uint8_t>(BUFSIZE);

	int target_id = -1;

	int initiator_id = 7;

	string filename;

	bool restore = false;
};
