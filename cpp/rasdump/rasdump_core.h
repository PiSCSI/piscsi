//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/scsi.h"
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

class RasDump
{
	static const char COMPONENT_SEPARATOR = ':';

	static const int MINIMUM_BUFFER_SIZE = 1024 * 64;
	static const int DEFAULT_BUFFER_SIZE = 1024 * 1024;

public:

	RasDump() = default;
	~RasDump() = default;

	int run(const vector<char *>&);

private:

	bool Banner(const vector<char *>&) const;
	bool Init() const;
	void ParseArguments(const vector<char *>&);
	int DumpRestore();
	void WaitPhase(BUS::phase_t) const;
	void Selection() const;
	void Command(scsi_defs::scsi_command, vector<uint8_t>&) const;
	void DataIn(int);
	void DataOut(int);
	void Status() const;
	void MessageIn() const;
	void MessageOut();
	void BusFree() const;
	void TestUnitReady() const;
	void RequestSense();
	void Inquiry();
	pair<uint64_t, uint32_t> ReadCapacity();
	void Read10(uint32_t, uint32_t, uint32_t);
	void Write10(uint32_t, uint32_t, uint32_t);
	void WaitForBusy() const;

	// TODO Use ras_util after removing its dependencies on protobuf interface.
	// Not required in case rasdump is integrated into rascsi.
	static void ProcessId(const string&, int&, int&);

	static void CleanUp();
	static void KillHandler(int);

	// A static instance is needed because of the signal handler
	static inline unique_ptr<BUS> bus;

	vector<uint8_t> buffer;

	int target_id = -1;

	int target_lun = 0;

	int initiator_id = 7;

	string filename;

	bool restore = false;
};
