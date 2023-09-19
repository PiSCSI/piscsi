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

	jthread service_thread;

	int service_socket = -1;

public:

	PiscsiService() = default;
	~PiscsiService() = default;

	bool Init(const callback&, int);
	void Start();
	void Stop();
	bool IsRunning() const { return service_socket != -1 && service_thread.joinable(); }

private:

	void Execute() const;
	void ExecuteCommand(int) const;
};
