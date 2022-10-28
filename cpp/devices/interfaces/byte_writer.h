//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Abstraction for the DaynaPort and host bridge
//
//---------------------------------------------------------------------------

#pragma once

#include "os.h"
#include <vector>

using namespace std;

class ByteWriter
{

public:

	ByteWriter() = default;
	virtual ~ByteWriter() = default;

	virtual bool WriteBytes(const vector<int>&, vector<BYTE>&, uint32_t) = 0;
};
