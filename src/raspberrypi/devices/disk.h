//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	XM6i
//	Copyright (C) 2010-2015 isaki@NetBSD.org
//
//  	Imported sava's Anex86/T98Next image and MO format support patch.
//  	Comments translated to english by akuker.
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi.h"
#include "device_factory.h"
#include "disk_track.h"
#include "disk_cache.h"
#include "filepath.h"
#include "interfaces/scsi_block_commands.h"
#include "mode_page_device.h"
#include <string>
#include <unordered_set>

using namespace std;

using id_set = pair<int, int>;

class Disk : public ModePageDevice, public ScsiBlockCommands
{
	enum access_mode { RW6, RW10, RW16, SEEK6, SEEK10 };

	Dispatcher<Disk> dispatcher;

	unique_ptr<DiskCache> cache;

	// The supported configurable sector sizes, empty if not configurable
	unordered_set<uint32_t> sector_sizes;
	uint32_t configured_sector_size = 0;

	// Sector size shift count (9=512, 10=1024, 11=2048, 12=4096)
	uint32_t size_shift_count = 0;

	// Total number of sectors
	uint64_t blocks = 0;

	bool is_medium_changed = false;

	Filepath diskpath;

	// The list of image files in use and the IDs and LUNs using these files
	static unordered_map<string, id_set> reserved_files;

public:

	Disk(const string&, int);
	~Disk() override;

	bool Dispatch(scsi_command) override;

	void MediumChanged();
	bool Eject(bool) override;

	// Command helpers
	virtual int WriteCheck(uint64_t);
	virtual void Write(const vector<int>&, const vector<BYTE>&, uint64_t);

	virtual int Read(const vector<int>&, vector<BYTE>& , uint64_t);

	uint32_t GetSectorSizeInBytes() const;
	bool IsSectorSizeConfigurable() const { return !sector_sizes.empty(); }
	bool SetConfiguredSectorSize(const DeviceFactory&, uint32_t);
	uint64_t GetBlockCount() const { return blocks; }
	void FlushCache() override;

	virtual void Open(const Filepath&);
	void GetPath(Filepath& path) const { path = diskpath; }

	void ReserveFile(const Filepath&, int, int) const;
	void UnreserveFile() const;
	static void UnreserveAll();
	bool FileExists(const Filepath&);

	static unordered_map<string, id_set> GetReservedFiles() { return reserved_files; }
	static void SetReservedFiles(const unordered_map<string, id_set>& files_in_use)	{ reserved_files = files_in_use; }
	static bool GetIdsForReservedFile(const Filepath&, int&, int&);

private:

	using super = ModePageDevice;

	// Commands covered by the SCSI specifications (see https://www.t10.org/drafts.htm)
	void StartStopUnit();
	void SendDiagnostic();
	void PreventAllowMediumRemoval();
	void SynchronizeCache();
	void ReadDefectData10();
	virtual void Read6();
	void Read10() override;
	void Read16() override;
	virtual void Write6();
	void Write10() override;
	void Write16() override;
	void Verify10();
	void Verify16();
	void Seek();
	void Seek10();
	void ReadCapacity10() override;
	void ReadCapacity16() override;
	void Reserve();
	void Release();
	void Rezero();
	void FormatUnit() override;
	void ReassignBlocks();
	void Seek6();
	void Read(access_mode);
	void Write(access_mode);
	void Verify(access_mode);
	void ReadWriteLong10();
	void ReadWriteLong16();
	void ReadCapacity16_ReadLong16();

	void ValidateBlockAddress(access_mode) const;
	bool CheckAndGetStartAndCount(uint64_t&, uint32_t&, access_mode) const;

	int ModeSense6(const vector<int>&, vector<BYTE>&) const override;
	int ModeSense10(const vector<int>&, vector<BYTE>&) const override;

protected:

	void SetUpCache(const Filepath&, off_t, bool = false);
	void ResizeCache(const Filepath&, bool);

	void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
	virtual void AddErrorPage(map<int, vector<byte>>&, bool) const;
	virtual void AddFormatPage(map<int, vector<byte>>&, bool) const;
	virtual void AddDrivePage(map<int, vector<byte>>&, bool) const;
	void AddCachePage(map<int, vector<byte>>&, bool) const;
	virtual void AddVendorPage(map<int, vector<byte>>&, int, bool) const;
	unordered_set<uint32_t> GetSectorSizes() const;
	void SetSectorSizes(const unordered_set<uint32_t>& sizes) { sector_sizes = sizes; }
	void SetSectorSizeInBytes(uint32_t);
	uint32_t GetSectorSizeShiftCount() const { return size_shift_count; }
	void SetSectorSizeShiftCount(uint32_t count) { size_shift_count = count; }
	uint32_t GetConfiguredSectorSize() const;
	void SetBlockCount(uint64_t b) { blocks = b; }
	void SetPath(const Filepath& path) { diskpath = path; }
};
