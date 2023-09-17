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

class CommandContext;

using namespace std;

class PiscsiService
{
	using callback = function<bool(const CommandContext&)>;

	callback execute;

	int service_socket = -1;

public:

	PiscsiService() = default;
	~PiscsiService() = default;

	bool Init(const callback&, int);
	void Start() const;
	void Cleanup();
	bool IsRunning() const { return service_socket != -1; }

private:

	void Execute() const;
	void ExecuteCommand(int) const;
};
