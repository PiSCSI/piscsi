//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <functional>
#include <thread>

class CommandContext;

using namespace std;

class PiscsiService
{
	using callback = function<bool(const CommandContext&)>;

	callback execute;

	int service_socket = -1;

	jthread command_executor;

public:

	PiscsiService() = default;
	~PiscsiService() = default;

	bool Init(const callback&, int);
	void Start();
	void Cleanup();
	bool IsRunning() const { return service_socket != -1; }

private:

	void Execute() const;
	void ExecuteCommand(int) const;
};
