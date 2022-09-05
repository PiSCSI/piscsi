//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// A template for dispatching SCSI commands
//
//---------------------------------------------------------------------------

#pragma once

#include "log.h"
#include "scsi.h"
#include <unordered_map>

using namespace std;
using namespace scsi_defs;

template<class T>
class Dispatcher
{
public:

	Dispatcher() : commands({}) {}
	~Dispatcher()
	{
		for (auto const& [name, command] : commands) {
			delete command;
		}
	}

	using operation = void (T::*)();
	using command_t = struct _command_t {
		const char* name;
		operation execute;

		_command_t(const char* _name, operation _execute) : name(_name), execute(_execute) { };
	};
	unordered_map<scsi_command, command_t*> commands;

	void AddCommand(scsi_command opcode, const char* name, operation execute)
	{
		commands[opcode] = new command_t(name, execute);
	}

	bool Dispatch(T *instance, DWORD cmd)
	{
		const auto& it = commands.find(static_cast<scsi_command>(cmd));
		if (it != commands.end()) {
			LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, it->second->name, (uint32_t)cmd)

			(instance->*it->second->execute)();

			return true;
		}

		// Unknown command
		return false;
	}
};
