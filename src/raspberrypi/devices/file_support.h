//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Devices inheriting from FileSupport support device image files
//
//---------------------------------------------------------------------------

#pragma once

#include "filepath.h"

class FileSupport
{
private:
	Filepath diskpath;

public:

	FileSupport() {};
	virtual ~FileSupport() {};

	void GetPath(Filepath& path) const { path = diskpath; }
	void SetPath(const Filepath& path) { diskpath = path; }

	virtual void Open(const Filepath&) = 0;
};
