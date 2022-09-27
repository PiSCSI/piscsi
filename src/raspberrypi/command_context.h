//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

class ProtobufSerializer;
class Localizer;

struct CommandContext
{
	CommandContext(const ProtobufSerializer& c, const Localizer& l, int f, const std::string& s)
		: connector(c), localizer(l), fd(f), locale(s) {}
	~CommandContext() = default;

	const ProtobufSerializer& connector;
	const Localizer& localizer;
	int fd;
	std::string locale;
};
