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

#include "xm6.h"
#include "log.h"
#include "scsi.h"
#include "controllers/scsidev_ctrl.h"
#include "primary_device.h"
#include "block_device.h"
#include "device.h"
#include "disk_track_cache.h"
#include "file_support.h"
#include "filepath.h"
#include <string>
#include <vector>
#include <map>

class Disk : public Device, public PrimaryDevice, public BlockDevice
{
private:
	enum access_mode { RW6, RW10, RW16 };

	vector<int> sector_sizes;
	int configured_sector_size;

	SASIDEV::ctrl_t *ctrl;

protected:
	// Internal data structure
	typedef struct {
		int size;								// Sector Size (8=256, 9=512, 10=1024, 11=2048, 12=4096)
		DWORD blocks;							// Total number of sectors
		DiskCache *dcache;						// Disk cache
		off_t imgoffset;						// Offset to actual data
	} disk_t;

private:
	typedef struct _command_t {
		const char* name;
		void (Disk::*execute)(SASIDEV *);

		_command_t(const char* _name, void (Disk::*_execute)(SASIDEV *)) : name(_name), execute(_execute) { };
	} command_t;
	std::map<SCSIDEV::scsi_command, command_t*> commands;

	void AddCommand(SCSIDEV::scsi_command, const char*, void (Disk::*)(SASIDEV *));

public:
	// Basic Functions
	Disk(std::string);							// Constructor
	virtual ~Disk();							// Destructor

	// Media Operations
	virtual void Open(const Filepath& path);	// Open
	void GetPath(Filepath& path) const;				// Get the path
	bool Eject(bool) override;					// Eject
	bool Flush();							// Flush the cache

	// Commands covered by the SCSI specification
	virtual void TestUnitReady(SASIDEV *) override;
	void Inquiry(SASIDEV *) override;
	void RequestSense(SASIDEV *) override;
	void ModeSelect(SASIDEV *);
	void ModeSelect10(SASIDEV *);
	void ModeSense(SASIDEV *);
	void ModeSense10(SASIDEV *);
	void Rezero(SASIDEV *);
	void FormatUnit(SASIDEV *) override;
	void ReassignBlocks(SASIDEV *);
	void StartStopUnit(SASIDEV *);
	void SendDiagnostic(SASIDEV *);
	void PreventAllowMediumRemoval(SASIDEV *);
	void SynchronizeCache10(SASIDEV *);
	void SynchronizeCache16(SASIDEV *);
	void ReadDefectData10(SASIDEV *);
	virtual void Read6(SASIDEV *);
	void Read10(SASIDEV *) override;
	void Read16(SASIDEV *) override;
	virtual void Write6(SASIDEV *);
	void Write10(SASIDEV *) override;
	void Write16(SASIDEV *) override;
	void Verify10(SASIDEV *) override;
	void Verify16(SASIDEV *) override;
	void Seek(SASIDEV *);
	void Seek6(SASIDEV *);
	void Seek10(SASIDEV *);
	void ReadCapacity10(SASIDEV *) override;
	void ReadCapacity16(SASIDEV *) override;
	void ReportLuns(SASIDEV *) override;
	void Reserve6(SASIDEV *);
	void Reserve10(SASIDEV *);
	void Release6(SASIDEV *);
	void Release10(SASIDEV *);

	// Command helpers
	virtual int Inquiry(const DWORD *cdb, BYTE *buf) = 0;	// INQUIRY command
	virtual int WriteCheck(DWORD block);					// WRITE check
	virtual bool Write(const DWORD *cdb, const BYTE *buf, DWORD block);			// WRITE command
	bool Assign(const DWORD *cdb);					// ASSIGN command
	bool StartStop(const DWORD *cdb);				// START STOP UNIT command
	bool SendDiag(const DWORD *cdb);				// SEND DIAGNOSTIC command
	bool Removal(const DWORD *cdb);				// PREVENT/ALLOW MEDIUM REMOVAL command

	virtual int Read(const DWORD *cdb, BYTE *buf, uint64_t block);
	virtual int ModeSense10(const DWORD *cdb, BYTE *buf);		// MODE SENSE(10) command
	int ReadDefectData10(const DWORD *cdb, BYTE *buf);		// READ DEFECT DATA(10) command
	int SelectCheck(const DWORD *cdb);				// SELECT check
	int SelectCheck10(const DWORD *cdb);				// SELECT(10) check

	int GetSectorSizeInBytes() const;
	void SetSectorSizeInBytes(int);
	int GetSectorSize() const;
	bool IsSectorSizeConfigurable() const;
	vector<int> GetSectorSizes() const;
	void SetSectorSizes(const vector<int>&);
	int GetConfiguredSectorSize() const;
	bool SetConfiguredSectorSize(int);
	uint32_t GetBlockCount() const;
	void SetBlockCount(DWORD);
	bool GetStartAndCount(SASIDEV *, uint64_t&, uint32_t&, access_mode);

	// TODO Try to get rid of this method, which is called by SASIDEV (but must not)
	virtual bool ModeSelect(const DWORD *cdb, const BYTE *buf, int length);// MODE SELECT command

	virtual bool Dispatch(SCSIDEV *) override;

protected:
	// Internal processing
	virtual int AddError(bool change, BYTE *buf);			// Add error
	virtual int AddFormat(bool change, BYTE *buf);			// Add format
	virtual int AddDrive(bool change, BYTE *buf);			// Add drive
	int AddOpt(bool change, BYTE *buf);				// Add optical
	int AddCache(bool change, BYTE *buf);				// Add cache
	int AddCDROM(bool change, BYTE *buf);				// Add CD-ROM
	int AddCDDA(bool, BYTE *buf);				// Add CD_DA
	virtual int AddVendor(int page, bool change, BYTE *buf);	// Add vendor special info
	bool CheckReady();						// Check if ready
	virtual int RequestSense(const DWORD *cdb, BYTE *buf);		// REQUEST SENSE command

	// Internal data
	disk_t disk;								// Internal disk data

private:
	void Read(SASIDEV *, uint64_t);
	void Write(SASIDEV *, uint64_t);
	void Verify(SASIDEV *, uint64_t);
	bool Format(const DWORD *cdb);					// FORMAT UNIT command
	int ModeSense(const DWORD *cdb, BYTE *buf);		// MODE SENSE command
};
