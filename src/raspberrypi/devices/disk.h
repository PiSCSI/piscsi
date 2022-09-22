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
//	[ Disk ]
//
//---------------------------------------------------------------------------

#pragma once

#include "log.h"
#include "scsi.h"
#include "device.h"
#include "device_factory.h"
#include "disk_track_cache.h"
#include "file_support.h"
#include "filepath.h"
#include "interfaces/scsi_block_commands.h"
#include "mode_page_device.h"
#include <string>
#include <unordered_set>

class Disk : public ModePageDevice, public ScsiBlockCommands
{
private:

	enum access_mode { RW6, RW10, RW16, SEEK6, SEEK10 };

	// The supported configurable block sizes, empty if not configurable
	unordered_set<uint32_t> sector_sizes;
	uint32_t configured_sector_size = 0;

	using disk_t = struct {
		uint32_t size;							// Sector Size (9=512, 10=1024, 11=2048, 12=4096)
		uint64_t blocks;						// Total number of sectors
		DiskCache *dcache;						// Disk cache
		off_t image_offset;						// Offset to actual data
		bool is_medium_changed;
	};

	Dispatcher<Disk> dispatcher;

public:

	explicit Disk(const string&);
	~Disk() override;
	Disk(Disk&) = delete;
	Disk& operator=(const Disk&) = delete;

	bool Dispatch(scsi_command) override;

	void MediumChanged();
	bool Eject(bool) override;

	// Command helpers
	virtual int WriteCheck(uint64_t);
	virtual void Write(const vector<int>&, const BYTE *, uint64_t);

	virtual int Read(const vector<int>&, BYTE *, uint64_t);

	uint32_t GetSectorSizeInBytes() const;
	bool IsSectorSizeConfigurable() const;
	bool SetConfiguredSectorSize(const DeviceFactory&, uint32_t);
	uint64_t GetBlockCount() const;
	void FlushCache() override;

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

	int ModeSense6(const vector<int>&, BYTE *, int) const override;
	int ModeSense10(const vector<int>&, BYTE *, int) const override;

protected:

	virtual void Open(const Filepath&);

	void AddModePages(map<int, vector<byte>>&, int, bool) const override;
	virtual void AddErrorPage(map<int, vector<byte>>&, bool) const;
	virtual void AddFormatPage(map<int, vector<byte>>&, bool) const;
	virtual void AddDrivePage(map<int, vector<byte>>&, bool) const;
	void AddCachePage(map<int, vector<byte>>&, bool) const;
	virtual void AddVendorPage(map<int, vector<byte>>&, int, bool) const;
	unordered_set<uint32_t> GetSectorSizes() const;
	void SetSectorSizes(const unordered_set<uint32_t>&);
	void SetSectorSizeInBytes(uint32_t);
	uint32_t GetSectorSizeShiftCount() const;
	void SetSectorSizeShiftCount(uint32_t);
	uint32_t GetConfiguredSectorSize() const;
	void SetBlockCount(uint64_t);

	// Internal disk data
	disk_t disk = {};
};
