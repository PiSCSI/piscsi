//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Abstraction for the DaynaPort and the host bridge, which both have methods for writing byte sequences
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>
#include <cstdint>

using namespace std;

class ByteWriter
{

public:

	ByteWriter() = default;
	virtual ~ByteWriter() = default;

	virtual bool WriteBytes(const vector<int>&, vector<uint8_t>&, uint32_t) = 0;
};
