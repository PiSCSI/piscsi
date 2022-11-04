//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include <vector>

using namespace std;

class RasDump
{
public:

	RasDump() = default;
	~RasDump() = default;

	int run(const vector<char *>&);

private:

	bool Banner(const vector<char *>&);
	bool Init();
	void Reset();
	bool ParseArguments(const vector<char *>&);
	bool WaitPhase(BUS::phase_t);
	void BusFree();
	bool Selection(int);
	bool Command(uint8_t *, int);
	int DataIn(uint8_t *, int);
	int DataOut(uint8_t *, int);
	int Status();
	int MessageIn();
	int TestUnitReady(int);
	int RequestSense(int, uint8_t *);
	int ModeSense(int, uint8_t *);
	int Inquiry(int, uint8_t *);
	int ReadCapacity(int, uint8_t *);
	int Read10(int, uint32_t, uint32_t, uint32_t, uint8_t *);
	int Write10(int, uint32_t, uint32_t, uint32_t, uint8_t *);
};
