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
#include <atomic>

class CommandContext;

using namespace std;

class PiscsiService
{
	using callback = function<bool(const CommandContext&)>;

	callback execute;

	int service_socket = -1;

	jthread monthread;

	static inline atomic<bool> running = false;

public:

	PiscsiService() = default;
	~PiscsiService() = default;

	bool Init(const callback&, int);
	void Cleanup() const;

	bool IsRunning() const { return running; }
	void SetRunning(bool b) const { running = b; }

private:

	void Execute() const;
};
