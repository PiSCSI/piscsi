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

	StorageDevice(PbDeviceType, int, const unordered_set<uint32_t>&);
	~StorageDevice() override = default;

	bool Init(const param_map&) override;
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

	static uint32_t CalculateShiftCount(uint32_t);
	uint32_t GetSectorSizeInBytes() const;
	void SetSectorSizeInBytes(uint32_t);
	const auto& GetSupportedSectorSizes() const { return supported_sector_sizes; }
	uint32_t GetMinSupportedSectorSize() const;
	uint32_t GetMaxSupportedSectorSize() const;

	unordered_set<uint32_t> GetSectorSizes() const;
	uint32_t GetSectorSizeShiftCount() const { return size_shift_count; }
	void SetSectorSizeShiftCount(uint32_t count) { size_shift_count = count; }
	uint32_t GetConfiguredSectorSize() const;

	virtual void Write(span<const uint8_t>, uint64_t) = 0;
	virtual int Read(span<uint8_t> , uint64_t) = 0;
protected:

	void ValidateFile();

	bool IsMediumChanged() const { return medium_changed; }

	void SetBlockCount(uint64_t b) { blocks = b; }

	off_t GetFileSize() const;

protected:
	// Sector size shift count (9=512, 10=1024, 11=2048, 12=4096)
	uint32_t size_shift_count = 0;

	unordered_set<uint32_t> supported_sector_sizes;

	static inline const unordered_map<uint32_t, uint32_t> shift_counts =
	{ { 512, 9 }, { 1024, 10 }, { 2048, 11 }, { 4096, 12 } };

	uint32_t configured_sector_size = 0;

private:
	void PreventAllowMediumRemoval();


private:

	bool IsReadOnlyFile() const;

	uint64_t blocks = 0;

	filesystem::path filename;

	bool medium_changed = false;

	// The list of image files in use and the IDs and LUNs using these files
	static inline unordered_map<string, id_set, piscsi_util::StringHash, equal_to<>> reserved_files;
};
