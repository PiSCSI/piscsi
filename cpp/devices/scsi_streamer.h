//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2024 BogDan Vatra <bogdan@kde.org>
//
//---------------------------------------------------------------------------

#pragma once

#include "storage_device.h"
#include <cstdio>

class File {
	FILE *file = nullptr;
	inline auto checkSize(auto size) const {
		if (size < 0)
			throw scsi_exception(sense_key::medium_error);
		return size;
	}

public:
	~File();
	bool open(std::string_view filename);
	void rewind();
	size_t read(uint8_t* buff, size_t size, size_t count = 1);
	size_t write(const uint8_t *buff, size_t size, size_t count = 1);
	void seek(long offset, int origin);
	long tell();
	void close();
};

class SCSIST: public StorageDevice
{
public:
	SCSIST(int lun);

public:
	bool Init(const param_map &pm) final;
	void CleanUp() final;

private:
	void Erase();
	void Read6();
	void ReadBlockLimits() const;
	void Rewind();
	void Space();
	void Write6();
	void WriteFilemarks();

	void LoadUnload();
	void ReadPosition();
	void Verify();

	int ModeSense6(cdb_t cdb, std::vector<uint8_t> &buf) const final;
	int ModeSense10(cdb_t, std::vector<uint8_t> &) const final;

	// PrimaryDevice interface
protected:
	std::vector<uint8_t> InquiryInternal() const final;

	// ModePageDevice interface
protected:
	void ModeSelect(scsi_defs::scsi_command, cdb_t, span<const uint8_t>, int) final;
	void SetUpModePages(std::map<int, std::vector<byte> > &, int, bool) const final;

	// StorageDevice interface
private:
	void Write(span<const uint8_t>, uint64_t) final;
	int Read(span<uint8_t> , uint64_t) final;

public:
	void Open() final;
	void AddBlockDescriptorPage(std::map<int, std::vector<byte> > &, bool) const;
	void AddErrorPage(map<int, vector<byte> > &, bool) const;
	void AddReconnectPage(map<int, vector<byte> > &, bool) const;
	void AddDevicePage(map<int, vector<byte> > &, bool) const;
	void AddMediumPartitionPage(map<int, vector<byte> > &, bool) const;
	void AddMiscellaneousPage(map<int, vector<byte> > &, bool) const;
private:
	File file;
};
