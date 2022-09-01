//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
// Devices inheriting from FileSupport support image files
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_map>
#include <string>
#include "filepath.h"

using namespace std;

typedef pair<int, int> id_set;

class FileSupport
{
	friend class ControllerManager;

	Filepath diskpath;

	// The list of image files in use and the IDs and LUNs using these files
	static unordered_map<string, id_set> reserved_files;

	static void UnreserveAll();

public:

	FileSupport() : diskpath({}) {}
	virtual ~FileSupport() {}

	void GetPath(Filepath& path) const { path = diskpath; }
	void SetPath(const Filepath& path) { diskpath = path; }

	void ReserveFile(const Filepath&, int, int);
	void UnreserveFile();

	static const unordered_map<string, id_set> GetReservedFiles() { return reserved_files; }
	static void SetReservedFiles(const unordered_map<string, id_set>& files_in_use)
		{ FileSupport::reserved_files = files_in_use; }
	static bool GetIdsForReservedFile(const Filepath&, int&, int&);

	virtual void Open(const Filepath&) = 0;
};
