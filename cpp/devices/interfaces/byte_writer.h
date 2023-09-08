//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// Abstraction for the DaynaPort and the host bridge, which both have methods for writing byte sequences
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <vector>

using namespace std;

class ByteWriter
{

public:

	ByteWriter() = default;
	virtual ~ByteWriter() = default;

	// TODO The vector should be a span, but the broken host bridge legacy code prevents that :-(
	virtual bool WriteBytes(span<const int>, vector<uint8_t>&) = 0;
};
