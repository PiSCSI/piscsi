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
#include <map>

class SASIDEV;
class SCSIDEV;

using namespace std;
using namespace scsi_defs;

template<class T, class U>
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
		void (T::*execute)(U *);

		_command_t(const char* _name, void (T::*_execute)(U *)) : name(_name), execute(_execute) { };
	} command_t;
	map<scsi_command, command_t*> commands;

	void AddCommand(scsi_command opcode, const char* name, void (T::*execute)(U *))
	{
		commands[opcode] = new command_t(name, execute);
	}

	bool Dispatch(T *instance, U *controller)
	{
		SASIDEV::ctrl_t *ctrl = controller->GetCtrl();
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
