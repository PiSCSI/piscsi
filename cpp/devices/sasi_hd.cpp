//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2026 Daniel Markstedt <daniel@mindani.net>
//
//---------------------------------------------------------------------------

#include "controllers/abstract_controller.h"
#include "sasi_hd.h"

using namespace scsi_defs;

SasiHd::SasiHd(int lun, const unordered_set<uint32_t>& sector_sizes) : Disk(SAHD, lun, sector_sizes)
{
	SetProduct("SASI HD");
	SetProtectable(true);
}

bool SasiHd::Init(const param_map& params)
{
	Disk::Init(params);

	// SASI uses opcode 0x05 for READ CAPACITY and has a legacy FORMAT opcode at 0x06.
	AddCommand(scsi_command::eCmdReadBlockLimits, [this] { ReadCapacity(); });
	AddCommand(scsi_command::eCmdFormatLegacy, [this] { Dispatch(scsi_command::eCmdFormatUnit); });
	AddCommand(scsi_command::eCmdTestUnitReady, [this] { TestUnitReady(); });
	AddCommand(scsi_command::eCmdAssignDiskParameters, [this] { AssignDiskParameters(); });

	return true;
}

void SasiHd::TestUnitReady()
{
	// X68000 SASI firmware probes a target with 00 28 and requires CHECK
	// CONDITION (with no sense data). 00 03 is the corresponding ready probe.
	if (GetController()->GetCmdByte(1) == 0x28) {
		GetController()->SetStatus(status::check_condition);
		EnterStatusPhase();
		return;
	}
	if (GetController()->GetCmdByte(1) == 0x03) {
		GetController()->SetStatus(status::good);
		EnterStatusPhase();
		return;
	}

	CheckReady();
	EnterStatusPhase();
}

void SasiHd::AssignDiskParameters()
{
	CheckReady();

	// SASI initiators such as the X68000 configure geometry with this
	// six-byte command followed by ten parameter bytes. Geometry is derived
	// from the image, so consume and ignore the parameter list.
	GetController()->SetLength(10);
	EnterDataOutPhase();
}

void SasiHd::Open()
{
	assert(!IsReady());

	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 256);
	SetBlockCount(GetFileSize() >> GetSectorSizeShiftCount());

	// READ/WRITE(6) encodes a 21-bit block address.
	if (GetBlockCount() > 2'097'152) {
		throw io_exception("SASI drives are limited to 2097152 blocks");
	}

	ValidateFile();
	SetUpCache(0);
}

void SasiHd::Inquiry()
{
	// SASI INQUIRY consists of two bytes; byte 0 denotes a direct-access device.
	GetController()->GetBuffer()[0] = 0;
	GetController()->GetBuffer()[1] = 0;
	GetController()->SetLength(2);
	EnterDataInPhase();
}

void SasiHd::RequestSense()
{
	// SASI uses the non-extended format and transfers four bytes when ALLOCATION LENGTH is zero.
	const int allocation_length = GetController()->GetCmdByte(4) ? GetController()->GetCmdByte(4) : 4;
	auto& buffer = GetController()->GetBuffer();
	fill_n(buffer.begin(), allocation_length, 0);
	buffer[0] = static_cast<uint8_t>(GetStatusCode() >> 16);
	buffer[1] = static_cast<uint8_t>(GetLun() << 5);
	GetController()->SetLength(allocation_length);
	EnterDataInPhase();
}

void SasiHd::ReadCapacity()
{
	CheckReady();

	const uint32_t capacity = static_cast<uint32_t>(GetBlockCount() - 1);
	auto& buffer = GetController()->GetBuffer();
	buffer[0] = static_cast<uint8_t>(capacity >> 24);
	buffer[1] = static_cast<uint8_t>(capacity >> 16);
	buffer[2] = static_cast<uint8_t>(capacity >> 8);
	buffer[3] = static_cast<uint8_t>(capacity);
	buffer[4] = static_cast<uint8_t>(GetSectorSizeInBytes() >> 8);
	buffer[5] = static_cast<uint8_t>(GetSectorSizeInBytes());
	GetController()->SetLength(6);
	EnterDataInPhase();
}
