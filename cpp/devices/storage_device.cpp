//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "storage_device.h"
#include <unistd.h>

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

	if (!exists(filename)) {
		throw file_not_found_exception("Image file '" + filename.string() + "' for " + GetTypeString() + " device does not exist");
	}

	if (GetFileSize() > 2LL * 1024 * 1024 * 1024 * 1024) {
		throw io_exception("Drives > 2 TiB are not supported");
	}

	// TODO Check for duplicate handling of these properties (-> piscsi_executor.cpp)
	if (IsReadOnlyFile()) {
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
	assert(!reserved_files.contains(file));

	reserved_files[file] = { id, lun };
}

void StorageDevice::UnreserveFile()
{
	reserved_files.erase(filename);

	filename.clear();
}

id_set StorageDevice::GetIdsForReservedFile(const string& file)
{
	if (const auto& it = reserved_files.find(file); it != reserved_files.end()) {
		return { it->second.first, it->second.second };
	}

	return { -1, -1 };
}

void StorageDevice::UnreserveAll()
{
	reserved_files.clear();
}

bool StorageDevice::FileExists(string_view file)
{
	return exists(path(file));
}

bool StorageDevice::IsReadOnlyFile() const
{
	return access(filename.c_str(), W_OK);
}

off_t StorageDevice::GetFileSize() const
{
	try {
		return file_size(filename);
	}
	catch (const filesystem_error& e) {
		throw io_exception("Can't get size of '" + filename.string() + "': " + e.what());
	}
}
