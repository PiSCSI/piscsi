//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// The base class for all mass storage devices with image file support
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/piscsi_util.h"
#include "mode_page_device.h"
#include <unordered_map>
#include <string>
#include <filesystem>

using namespace std;

class StorageDevice : public ModePageDevice
{
public:

	StorageDevice(PbDeviceType, int);
	~StorageDevice() override = default;

	void CleanUp() override;

	virtual void Open() = 0;

	string GetFilename() const { return filename.string(); }
	void SetFilename(string_view);

	uint64_t GetBlockCount() const { return blocks; }

	void ReserveFile() const;
	void UnreserveFile();
	// TODO Remove this method, it is only used by the unit tests
	static void UnreserveAll();

	static bool FileExists(string_view);

	void SetMediumChanged(bool b) { medium_changed = b; }

	static auto GetReservedFiles() { return reserved_files; }
	static void SetReservedFiles(const unordered_map<string, id_set, piscsi_util::StringHash, equal_to<>>& r)
		{ reserved_files = r; }
	static id_set GetIdsForReservedFile(const string&);

protected:

	void ValidateFile();

	bool IsMediumChanged() const { return medium_changed; }

	void SetBlockCount(uint64_t b) { blocks = b; }

	off_t GetFileSize() const;

private:

	bool IsReadOnlyFile() const;

	uint64_t blocks = 0;

	filesystem::path filename;

	bool medium_changed = false;

	// The list of image files in use and the IDs and LUNs using these files
	static inline unordered_map<string, id_set, piscsi_util::StringHash, equal_to<>> reserved_files;
};
