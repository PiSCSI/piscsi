//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// Abstraction for devices writing byte sequences
//
//---------------------------------------------------------------------------

#pragma once

#include <span>

using namespace std;

class ByteWriter
{

public:

	ByteWriter() = default;
	virtual ~ByteWriter() = default;

	virtual bool WriteBytes(span<const int>, span<const uint8_t>) = 0;
};
