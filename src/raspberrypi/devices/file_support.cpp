//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "file_support.h"

using namespace std;

unordered_map<string, id_set> FileSupport::reserved_files;

void FileSupport::ReserveFile(const Filepath& path, int id, int lun) const
{
	reserved_files[path.GetPath()] = make_pair(id, lun);
}

void FileSupport::UnreserveFile()
{
	reserved_files.erase(diskpath.GetPath());
}

bool FileSupport::GetIdsForReservedFile(const Filepath& path, int& id, int& lun)
{
	const auto& it = reserved_files.find(path.GetPath());
	if (it != reserved_files.end()) {
		id = it->second.first;
		lun = it->second.second;

		return true;
	}

	return false;
}

void FileSupport::UnreserveAll()
{
	reserved_files.clear();
}
