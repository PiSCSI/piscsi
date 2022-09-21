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
	CommandContext(SocketConnector *c, Localizer *l, int f, std::string s)
		: connector(c), localizer(l), fd(f), locale(s) {}
	~CommandContext() = default;

	SocketConnector *connector;
	Localizer *localizer;
	int fd;
	std::string locale;
};
