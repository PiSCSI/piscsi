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

class SocketConnector;
class Localizer;

struct CommandContext
{
	CommandContext(const SocketConnector& c, const Localizer& l, int f, const std::string& s)
		: connector(c), localizer(l), fd(f), locale(s) {}
	~CommandContext() = default;

	const SocketConnector& connector;
	const Localizer& localizer;
	int fd;
	std::string locale;
};
