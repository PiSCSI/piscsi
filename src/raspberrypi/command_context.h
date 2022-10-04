//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

class ProtobufSerializer;
class Localizer;

class CommandContext
{
public:

	CommandContext(const ProtobufSerializer& c, const Localizer& l, int f, const std::string& s)
		: serializer(c), localizer(l), fd(f), locale(s) {}
	~CommandContext() = default;

	const ProtobufSerializer& serializer;
	const Localizer& localizer;
	int fd;
	std::string locale;
};
