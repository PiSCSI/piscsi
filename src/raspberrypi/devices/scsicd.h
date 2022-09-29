//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License.
//	See LICENSE file in the project root folder.
//
//	[ SCSI CD-ROM  ]
//
//---------------------------------------------------------------------------

#pragma once

#include "disk.h"
#include "filepath.h"
#include "cd_track.h"
#include "file_support.h"
#include "interfaces/scsi_mmc_commands.h"
#include "interfaces/scsi_primary_commands.h"

class SCSICD : public Disk, public ScsiMmcCommands, public FileSupport
{
public:

	SCSICD(int, int, const unordered_set<uint32_t>&);
	~SCSICD() override = default;

	bool Dispatch(scsi_command) override;

	void Open(const Filepath& path) override;

	// Commands
	vector<byte> InquiryInternal() const override;
	int Read(const vector<int>&, vector<BYTE>&, uint64_t) override;

protected:

	void SetUpModePages(map<int, vector<byte>>&, int, bool) const override;
	void AddVendorPage(map<int, vector<byte>>&, int, bool) const override;

private:

	using super = Disk;

	Dispatcher<SCSICD> dispatcher;

	int ReadTocInternal(const vector<int>&, vector<BYTE>&);

	void AddCDROMPage(map<int, vector<byte>>&, bool) const;
	void AddCDDAPage(map<int, vector<byte>>&, bool) const;

	// Open
	void OpenCue(const Filepath& path) const;
	void OpenIso(const Filepath& path);
	void OpenPhysical(const Filepath& path);

	void ReadToc() override;
	void GetEventStatusNotification() override;

	void LBAtoMSF(uint32_t, BYTE *) const;			// LBA→MSF conversion

	bool rawfile = false;					// RAW flag

	// Track management
	void ClearTrack();						// Clear the track
	int SearchTrack(DWORD lba) const;		// Track search
	vector<unique_ptr<CDTrack>> tracks;		// Track opbject references
	int dataindex = -1;						// Current data track
	int audioindex = -1;					// Current audio track
};
