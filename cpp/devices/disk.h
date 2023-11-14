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
//  	Comments translated to english by akuker.
//
//---------------------------------------------------------------------------

#pragma once

#include "shared/scsi.h"
#include "shared/piscsi_util.h"
#include "disk_track.h"
#include "disk_cache.h"
#include "interfaces/scsi_block_commands.h"
#include "storage_device.h"
#include <string>
#include <span>
#include <unordered_set>
#include <unordered_map>
#include <tuple>

using namespace std;

class Disk : public StorageDevice, private ScsiBlockCommands
{
	enum access_mode { RW6, RW10, RW16, SEEK6, SEEK10 };

	unique_ptr<DiskCache> cache;

	unordered_set<uint32_t> supported_sector_sizes;
	uint32_t configured_sector_size = 0;

	// Sector size shift count (9=512, 10=1024, 11=2048, 12=4096)
	uint32_t size_shift_count = 0;

	uint64_t sector_read_count = 0;
	uint64_t sector_write_count = 0;

	inline static const string SECTOR_READ_COUNT = "sector_read_count";
	inline static const string SECTOR_WRITE_COUNT = "sector_write_count";

public:

	Disk(PbDeviceType type, int lun, const unordered_set<uint32_t>& s)
		: StorageDevice(type, lun), supported_sector_sizes(s) {}
	~Disk() override = default;

	bool Init(const param_map&) override;
	void CleanUp() override;

	void Dispatch(scsi_command) override;

	bool Eject(bool) override;

	virtual void Write(span<const uint8_t>, uint64_t);

	virtual int Read(span<uint8_t> , uint64_t);

	uint32_t GetSectorSizeInBytes() const;
	bool IsSectorSizeConfigurable() const { return supported_sector_sizes.size() > 1; }
	const auto& GetSupportedSectorSizes() const { return supported_sector_sizes; }
	bool SetConfiguredSectorSize(uint32_t);
	void FlushCache() override;

	vector<PbStatistics> GetStatistics() const override;

private:

	// Commands covered by the SCSI specifications (see https://www.t10.org/drafts.htm)
	void StartStopUnit();
	void PreventAllowMediumRemoval();
	void SynchronizeCache();
	void ReadDefectData10() const;
	virtual void Read6() { Read(RW6); }
	void Read10() override { Read(RW10); }
	void Read16() override { Read(RW16); }
	virtual void Write6() { Write(RW6); }
	void Write10() override { Write(RW10); }
	void Write16() override { Write(RW16); }
	void Verify10() { Verify(RW10); }
	void Verify16() { Verify(RW16); }
	void Seek();
	void Seek10();
	void ReadCapacity10() override;
	void ReadCapacity16() override;
	void FormatUnit() override;
	void Seek6();
	void Read(access_mode);
	void Write(access_mode) const;
	void Verify(access_mode);
	void ReadWriteLong10() const;
	void ReadWriteLong16() const;
	void ReadCapacity16_ReadLong16();

	void ValidateBlockAddress(access_mode) const;
	tuple<bool, uint64_t, uint32_t> CheckAndGetStartAndCount(access_mode) const;

	int ModeSense6(cdb_t, vector<uint8_t>&) const override;
	int ModeSense10(cdb_t, vector<uint8_t>&) const override;

	static inline const unordered_map<uint32_t, uint32_t> shift_counts =
		{ { 512, 9 }, { 1024, 10 }, { 2048, 11 }, { 4096, 12 } };

protected:

	void SetUpCache(off_t, bool = false);
	void ResizeCache(const string&, bool);

	void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
	void AddErrorPage(map<int, vector<byte>>&, bool) const;
	virtual void AddFormatPage(map<int, vector<byte>>&, bool) const;
	virtual void AddDrivePage(map<int, vector<byte>>&, bool) const;
	void AddCachePage(map<int, vector<byte>>&, bool) const;

	unordered_set<uint32_t> GetSectorSizes() const;
	void SetSectorSizeInBytes(uint32_t);
	uint32_t GetSectorSizeShiftCount() const { return size_shift_count; }
	void SetSectorSizeShiftCount(uint32_t count) { size_shift_count = count; }
	uint32_t GetConfiguredSectorSize() const;
	static uint32_t CalculateShiftCount(uint32_t);
};
