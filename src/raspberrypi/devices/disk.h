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
#include "controller.h"
#include "device.h"
#include "device_factory.h"
#include "disk_track_cache.h"
#include "file_support.h"
#include "filepath.h"
#include "interfaces/scsi_block_commands.h"
#include "mode_page_device.h"
#include <string>
#include <unordered_set>
#include <unordered_map>

using namespace std;

class Disk : public ModePageDevice, ScsiBlockCommands
{
private:
	enum access_mode { RW6, RW10, RW16, SEEK6, SEEK10 };

	// The supported configurable block sizes, empty if not configurable
	unordered_set<uint32_t> sector_sizes;
	uint32_t configured_sector_size;

	// The mapping of supported capacities to block sizes and block counts, empty if there is no capacity restriction
	unordered_map<uint64_t, Geometry> geometries;

	typedef struct {
		uint32_t size;							// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
		// TODO blocks should be a 64 bit value in order to support higher capacities
		uint32_t blocks;						// Total number of sectors
		DiskCache *dcache;						// Disk cache
		off_t image_offset;						// Offset to actual data
		bool is_medium_changed;
	} disk_t;

	Dispatcher<Disk> dispatcher;

public:
	Disk(const string&);
	virtual ~Disk();

	virtual bool Dispatch(Controller *) override;

	void MediumChanged();
	void ReserveFile(const string&);

	// Media Operations
	virtual void Open(const Filepath& path);
	void GetPath(Filepath& path) const;
	bool Eject(bool) override;

private:
	friend class Controller;

	typedef ModePageDevice super;

	// Commands covered by the SCSI specification (see https://www.t10.org/drafts.htm)
	void StartStopUnit(Controller *);
	void SendDiagnostic(Controller *);
	void PreventAllowMediumRemoval(Controller *);
	void SynchronizeCache10(Controller *);
	void SynchronizeCache16(Controller *);
	void ReadDefectData10(Controller *);
	virtual void Read6(Controller *);
	void Read10(Controller *) override;
	void Read16(Controller *) override;
	virtual void Write6(Controller *);
	void Write10(Controller *) override;
	void Write16(Controller *) override;
	void Verify10(Controller *);
	void Verify16(Controller *);
	void Seek(Controller *);
	void Seek10(Controller *);
	virtual void ReadCapacity10(Controller *) override;
	void ReadCapacity16(Controller *) override;
	void Reserve(Controller *);
	void Release(Controller *);

public:

	// Commands covered by the SCSI specification (see https://www.t10.org/drafts.htm)
	void Rezero(Controller *);
	void FormatUnit(Controller *) override;
	void ReassignBlocks(Controller *);
	void Seek6(Controller *);

	// Command helpers
	virtual int WriteCheck(DWORD block);
	virtual void Write(Controller *, const DWORD *cdb, BYTE *buf, uint64_t block);
	bool StartStop(const DWORD *cdb);
	bool SendDiag(const DWORD *cdb) const;

	virtual int Read(const DWORD *cdb, BYTE *buf, uint64_t block);

	uint32_t GetSectorSizeInBytes() const;
	void SetSectorSizeInBytes(uint32_t);
	uint32_t GetSectorSizeShiftCount() const;
	void SetSectorSizeShiftCount(uint32_t);
	bool IsSectorSizeConfigurable() const;
	unordered_set<uint32_t> GetSectorSizes() const;
	void SetSectorSizes(const unordered_set<uint32_t>&);
	uint32_t GetConfiguredSectorSize() const;
	bool SetConfiguredSectorSize(uint32_t);
	void SetGeometries(const unordered_map<uint64_t, Geometry>&);
	bool SetGeometryForCapacity(uint64_t);
	uint64_t GetBlockCount() const;
	void SetBlockCount(uint32_t);
	void FlushCache();

protected:

	int ModeSense6(const DWORD *cdb, BYTE *buf);
	int ModeSense10(const DWORD *cdb, BYTE *buf, int);
	virtual void SetDeviceParameters(BYTE *);
	void AddModePages(map<int, vector<BYTE>>&, int, bool) const override;
	virtual void AddErrorPage(map<int, vector<BYTE>>&, bool) const;
	virtual void AddFormatPage(map<int, vector<BYTE>>&, bool) const;
	virtual void AddDrivePage(map<int, vector<BYTE>>&, bool) const;
	void AddCachePage(map<int, vector<BYTE>>&, bool) const;
	virtual void AddVendorPage(map<int, vector<BYTE>>&, int, bool) const;

	// Internal disk data
	disk_t disk;

private:

	void Read(Controller *, access_mode);
	void Write(Controller *, access_mode);
	void Verify(Controller *, access_mode);
	void ReadWriteLong10(Controller *);
	void ReadWriteLong16(Controller *);
	void ReadCapacity16_ReadLong16(Controller *);
	void Format(const DWORD *cdb);

	void ValidateBlockAddress(Controller *, access_mode);
	bool CheckAndGetStartAndCount(Controller *, uint64_t&, uint32_t&, access_mode);
};
