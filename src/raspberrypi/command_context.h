//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Helper methods for serializing/deserializing protobuf messages
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

struct CommandContext {
	int fd;
	std::string locale;
};
