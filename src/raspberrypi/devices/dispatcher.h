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
#include <unordered_map>

using namespace std; //NOSONAR Not relevant for rascsi
using namespace scsi_defs; //NOSONAR Not relevant for rascsi

template<class T>
class Dispatcher
{
public:

	Dispatcher() = default;
	~Dispatcher() = default;

	using operation = void (T::*)();
	using command_t = struct _command_t {
		const char *name;
		operation execute;

		_command_t(const char *_name, operation _execute) : name(_name), execute(_execute) { };
	};
	unordered_map<scsi_command, unique_ptr<command_t>> commands;

	void Add(scsi_command opcode, const char *name, operation execute)
	{
		commands[opcode] = make_unique<command_t>(name, execute);
	}

	bool Dispatch(T *instance, scsi_command cmd)
	{
		if (const auto& it = commands.find(cmd); it != commands.end()) {
			LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, it->second->name, (int)cmd)

			(instance->*it->second->execute)();

			return true;
		}

		// Unknown command
		return false;
	}
};
