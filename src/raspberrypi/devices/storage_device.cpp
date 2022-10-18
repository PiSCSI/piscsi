//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rascsi_exceptions.h"
#include "storage_device.h"
#include <unistd.h>
#include <filesystem>

using namespace std;
using namespace filesystem;

unordered_map<string, id_set> StorageDevice::reserved_files;

StorageDevice::StorageDevice(PbDeviceType type, int lun) : ModePageDevice(type, lun)
{
	SetStoppable(true);
}

void StorageDevice::ValidateFile(const string& file)
{
	if (blocks == 0) {
		throw io_exception(string(GetTypeString()) + " device has 0 blocks");
	}

	// TODO Check for duplicate handling of these properties (-> rascsi_executor.cpp)
	if (access(file.c_str(), W_OK)) {
		// Permanently write-protected
		SetReadOnly(true);
		SetProtectable(false);
		SetProtected(false);
	}

	SetStopped(false);
	SetRemoved(false);
	SetLocked(false);
	SetReady(true);
}

void StorageDevice::MediumChanged()
{
	assert(IsRemovable());

	medium_changed = true;
}

void StorageDevice::ReserveFile(const string& file, int id, int lun) const
{
	reserved_files[file] = make_pair(id, lun);
}

void StorageDevice::UnreserveFile()
{
	reserved_files.erase(filename);

	filename = "";
}

bool StorageDevice::GetIdsForReservedFile(const string& file, int& id, int& lun)
{
	if (const auto& it = reserved_files.find(file); it != reserved_files.end()) {
		id = it->second.first;
		lun = it->second.second;

		return true;
	}

	return false;
}

void StorageDevice::UnreserveAll()
{
	reserved_files.clear();
}

bool StorageDevice::FileExists(const string& file)
{
	return exists(file);
}

bool StorageDevice::IsReadOnlyFile() const
{
	return access(filename.c_str(), W_OK);
}

off_t StorageDevice::GetFileSize() const
{
	// filesystem::file_size cannot be used here because gcc < 10.3.0 cannot handled more than 2 GiB
	if (struct stat st; !stat(filename.c_str(), &st)) {
		return st.st_size;
	}

	throw io_exception("Can't get file size");
}
