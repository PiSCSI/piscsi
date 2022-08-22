//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
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
#include "controller.h"
#include <unordered_map>

class Controller;

using namespace std;
using namespace scsi_defs;

template<class T>
class Dispatcher
{
public:

	Dispatcher() {}
	~Dispatcher()
	{
		for (auto const& command : commands) {
			delete command.second;
		}
	}

	typedef struct _command_t {
		const char* name;
		void (T::*execute)(Controller *);

		_command_t(const char* _name, void (T::*_execute)(Controller *)) : name(_name), execute(_execute) { };
	} command_t;
	unordered_map<scsi_command, command_t*> commands;

	void AddCommand(scsi_command opcode, const char* name, void (T::*execute)(Controller *))
	{
		commands[opcode] = new command_t(name, execute);
	}

	bool Dispatch(T *instance, Controller *controller)
	{
		Controller::ctrl_t *ctrl = controller->GetCtrl();
		instance->SetCtrl(ctrl);

		const auto& it = commands.find(static_cast<scsi_command>(ctrl->cmd[0]));
		if (it != commands.end()) {
			LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, it->second->name, (unsigned int)ctrl->cmd[0]);

			(instance->*it->second->execute)(controller);

			return true;
		}

		// Unknown command
		return false;
	}
};
