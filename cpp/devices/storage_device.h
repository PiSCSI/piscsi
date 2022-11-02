//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// The base class for all mass storage devices with image file support
//
//---------------------------------------------------------------------------

#pragma once

#include "mode_page_device.h"
#include <unordered_map>
#include <string>

using namespace std;

using id_set = pair<int, int>;

class StorageDevice : public ModePageDevice
{
public:

	StorageDevice(PbDeviceType, int);
	~StorageDevice() override = default;

	virtual void Open() = 0;

	string GetFilename() const { return filename; }
	void SetFilename(string_view f) { filename = f; }

	void MediumChanged();

	uint64_t GetBlockCount() const { return blocks; }

	void ReserveFile(const string&, int, int) const;
	void UnreserveFile();
	static void UnreserveAll();

	static bool FileExists(const string&);
	bool IsReadOnlyFile() const;

	static unordered_map<string, id_set> GetReservedFiles() { return reserved_files; }
	static void SetReservedFiles(const unordered_map<string, id_set>& r) { reserved_files = r; }
	static pair<int, int> GetIdsForReservedFile(const string&);

protected:

	void ValidateFile();

	bool IsMediumChanged() const { return medium_changed; }
	void SetMediumChanged(bool b) { medium_changed = b; }

	void SetBlockCount(uint64_t b) { blocks = b; }

	off_t GetFileSize() const;

private:

	// Total number of blocks
	uint64_t blocks = 0;

	string filename;

	bool medium_changed = false;

	// The list of image files in use and the IDs and LUNs using these files
	static inline unordered_map<string, id_set> reserved_files;
};
