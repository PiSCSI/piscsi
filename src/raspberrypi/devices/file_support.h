//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Devices inheriting from FileSupport support image files
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include <string>
#include "filepath.h"

using namespace std;

typedef pair<int, int> id_set;

class FileSupport
{
private:
	Filepath diskpath;

	// The list of image files in use and the IDs and LUNs using these files
	static map<string, id_set> reserved_files;

public:

	FileSupport() {};
	virtual ~FileSupport() {};

	void GetPath(Filepath& path) const { path = diskpath; }
	void SetPath(const Filepath& path) { diskpath = path; }
	static const map<string, id_set> GetReservedFiles(){ return reserved_files; }
	static void SetReservedFiles(const map<string, id_set>& files_in_use) { FileSupport::reserved_files = files_in_use; }
	void ReserveFile(const Filepath&, int, int);
	void UnreserveFile();

	static bool GetIdsForReservedFile(const Filepath&, int&, int&);
	static void UnreserveAll();

	virtual void Open(const Filepath&) = 0;
};
