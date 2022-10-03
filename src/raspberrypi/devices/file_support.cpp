//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rascsi_exceptions.h"
#include "file_support.h"
#include <filesystem>

using namespace std;

unordered_map<string, id_set> FileSupport::reserved_files;

void FileSupport::ReserveFile(const Filepath& path, int id, int lun) const
{
	reserved_files[path.GetPath()] = make_pair(id, lun);
}

void FileSupport::UnreserveFile() const
{
	reserved_files.erase(diskpath.GetPath());
}

bool FileSupport::GetIdsForReservedFile(const Filepath& path, int& id, int& lun)
{
	if (const auto& it = reserved_files.find(path.GetPath()); it != reserved_files.end()) {
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

bool FileSupport::FileExists(const Filepath& filepath) const
{
	return filesystem::exists(filepath.GetPath());
}
