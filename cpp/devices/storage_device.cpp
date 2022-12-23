//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "storage_device.h"
#include <sys/stat.h>
#include <unistd.h>
#include <filesystem>

using namespace std;
using namespace filesystem;

StorageDevice::StorageDevice(PbDeviceType type, int lun) : ModePageDevice(type, lun)
{
	SupportsFile(true);
	SetStoppable(true);
}

void StorageDevice::ValidateFile()
{
	if (blocks == 0) {
		throw io_exception(string(GetTypeString()) + " device has 0 blocks");
	}

	if (!exists(path(filename))) {
		throw file_not_found_exception("Image file '" + filename + "' for " + GetTypeString() + " device does not exist");
	}

	if (GetFileSize() > 2LL * 1024 * 1024 * 1024 * 1024) {
		throw io_exception("Drive capacity cannot exceed 2 TiB");
	}

	// TODO Check for duplicate handling of these properties (-> piscsi_executor.cpp)
	if (access(filename.c_str(), W_OK)) {
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

void StorageDevice::ReserveFile(const string& file, int id, int lun) const
{
	assert(!file.empty());
	assert(reserved_files.find(file) == reserved_files.end());

	reserved_files[file] = make_pair(id, lun);
}

void StorageDevice::UnreserveFile()
{
	reserved_files.erase(filename);

	filename = "";
}

id_set StorageDevice::GetIdsForReservedFile(const string& file)
{
	if (const auto& it = reserved_files.find(file); it != reserved_files.end()) {
		return make_pair(it->second.first, it->second.second);
	}

	return make_pair(-1, -1);
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
	// filesystem::file_size cannot be used here because gcc < 10.3.0 cannot handle more than 2 GiB
	if (struct stat st; !stat(filename.c_str(), &st)) {
		return st.st_size;
	}

	throw io_exception("Can't get size of '" + filename + "'");
}
