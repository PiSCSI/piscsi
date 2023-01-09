//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <map>
#include <string>
#include "file_support.h"

using namespace std;

map<string, id_set> FileSupport::reserved_files;

void FileSupport::ReserveFile(const Filepath& path, int id, int lun)
{
	reserved_files[path.GetPath()] = make_pair(id, lun);
}

void FileSupport::UnreserveFile()
{
	reserved_files.erase(diskpath.GetPath());
}

bool FileSupport::GetIdsForReservedFile(const Filepath& path, int& id, int& unit)
{
	if (reserved_files.find(path.GetPath()) != reserved_files.end()) {
		const id_set ids = reserved_files[path.GetPath()];
		id = ids.first;
		unit = ids.second;
		return true;
	}

	return false;
}

void FileSupport::UnreserveAll()
{
	reserved_files.clear();
}
