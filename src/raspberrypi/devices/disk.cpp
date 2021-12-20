//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//	XM6i
//	Copyright (C) 2010-2015 isaki@NetBSD.org
//	Copyright (C) 2010 Y.Sugahara
//
//	Imported sava's Anex86/T98Next image and MO format support patch.
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
//  	Comments translated to english by akuker.
//
//---------------------------------------------------------------------------

#include "os.h"
#include "controllers/sasidev_ctrl.h"
#include "device_factory.h"
#include "exceptions.h"
#include "disk.h"
#include <sstream>
#include "../rascsi.h"

Disk::Disk(const std::string id) : Device(id), ScsiPrimaryCommands(), ScsiBlockCommands()
{
	// Work initialization
	configured_sector_size = 0;
	disk.size = 0;
	disk.blocks = 0;
	disk.dcache = NULL;
	disk.image_offset = 0;

	AddCommand(SCSIDEV::eCmdTestUnitReady, "TestUnitReady", &Disk::TestUnitReady);
	AddCommand(SCSIDEV::eCmdRezero, "Rezero", &Disk::Rezero);
	AddCommand(SCSIDEV::eCmdRequestSense, "RequestSense", &Disk::RequestSense);
	AddCommand(SCSIDEV::eCmdFormat, "FormatUnit", &Disk::FormatUnit);
	AddCommand(SCSIDEV::eCmdReassign, "ReassignBlocks", &Disk::ReassignBlocks);
	AddCommand(SCSIDEV::eCmdRead6, "Read6", &Disk::Read6);
	AddCommand(SCSIDEV::eCmdWrite6, "Write6", &Disk::Write6);
	AddCommand(SCSIDEV::eCmdSeek6, "Seek6", &Disk::Seek6);
	AddCommand(SCSIDEV::eCmdInquiry, "Inquiry", &Disk::Inquiry);
	AddCommand(SCSIDEV::eCmdModeSelect6, "ModeSelect6", &Disk::ModeSelect6);
	AddCommand(SCSIDEV::eCmdReserve6, "Reserve6", &Disk::Reserve6);
	AddCommand(SCSIDEV::eCmdRelease6, "Release6", &Disk::Release6);
	AddCommand(SCSIDEV::eCmdModeSense6, "ModeSense6", &Disk::ModeSense6);
	AddCommand(SCSIDEV::eCmdStartStop, "StartStopUnit", &Disk::StartStopUnit);
	AddCommand(SCSIDEV::eCmdSendDiag, "SendDiagnostic", &Disk::SendDiagnostic);
	AddCommand(SCSIDEV::eCmdRemoval, "PreventAllowMediumRemoval", &Disk::PreventAllowMediumRemoval);
	AddCommand(SCSIDEV::eCmdReadCapacity10, "ReadCapacity10", &Disk::ReadCapacity10);
	AddCommand(SCSIDEV::eCmdRead10, "Read10", &Disk::Read10);
	AddCommand(SCSIDEV::eCmdWrite10, "Write10", &Disk::Write10);
	AddCommand(SCSIDEV::eCmdReadLong10, "ReadLong10", &Disk::ReadLong10);
	AddCommand(SCSIDEV::eCmdWriteLong10, "WriteLong10", &Disk::WriteLong10);
	AddCommand(SCSIDEV::eCmdWriteLong16, "WriteLong16", &Disk::WriteLong16);
	AddCommand(SCSIDEV::eCmdSeek10, "Seek10", &Disk::Seek10);
	AddCommand(SCSIDEV::eCmdVerify10, "Verify10", &Disk::Verify10);
	AddCommand(SCSIDEV::eCmdSynchronizeCache10, "SynchronizeCache10", &Disk::SynchronizeCache10);
	AddCommand(SCSIDEV::eCmdSynchronizeCache16, "SynchronizeCache16", &Disk::SynchronizeCache16);
	AddCommand(SCSIDEV::eCmdReadDefectData10, "ReadDefectData10", &Disk::ReadDefectData10);
	AddCommand(SCSIDEV::eCmdModeSelect10, "ModeSelect10", &Disk::ModeSelect10);
	AddCommand(SCSIDEV::eCmdReserve10, "Reserve10", &Disk::Reserve10);
	AddCommand(SCSIDEV::eCmdRelease10, "Release10", &Disk::Release10);
	AddCommand(SCSIDEV::eCmdModeSense10, "ModeSense10", &Disk::ModeSense10);
	AddCommand(SCSIDEV::eCmdRead16, "Read16", &Disk::Read16);
	AddCommand(SCSIDEV::eCmdWrite16, "Write16", &Disk::Write16);
	AddCommand(SCSIDEV::eCmdVerify16, "Verify16", &Disk::Verify16);
	AddCommand(SCSIDEV::eCmdReadCapacity16_ReadLong16, "ReadCapacity16/ReadLong16", &Disk::ReadCapacity16_ReadLong16);
	AddCommand(SCSIDEV::eCmdReportLuns, "ReportLuns", &Disk::ReportLuns);
}

Disk::~Disk()
{
	// Save disk cache
	if (IsReady()) {
		// Only if ready...
		if (disk.dcache) {
			disk.dcache->Save();
		}
	}

	// Clear disk cache
	if (disk.dcache) {
		delete disk.dcache;
		disk.dcache = NULL;
	}

	for (auto const& command : commands) {
		delete command.second;
	}
}

void Disk::AddCommand(SCSIDEV::scsi_command opcode, const char* name, void (Disk::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}

bool Disk::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	if (commands.count(static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0]))) {
		command_t *command = commands[static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0])];

		LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, command->name, (unsigned int)ctrl->cmd[0]);

		(this->*command->execute)(controller);

		return true;
	}

	// Unknown command
	return false;
}

//---------------------------------------------------------------------------
//
//	Open
//  * Call as a post-process after successful opening in a derived class
//
//---------------------------------------------------------------------------
void Disk::Open(const Filepath& path)
{
	ASSERT(disk.blocks > 0);

	SetReady(true);

	// Cache initialization
	assert (!disk.dcache);
	disk.dcache = new DiskCache(path, disk.size, disk.blocks, disk.image_offset);

	// Can read/write open
	Fileio fio;
	if (fio.Open(path, Fileio::ReadWrite)) {
		// Write permission
		fio.Close();
	} else {
		// Permanently write-protected
		SetReadOnly(true);
		SetProtectable(false);
		SetProtected(false);
	}

	SetStopped(false);
	SetRemoved(false);
	SetLocked(false);
}

void Disk::TestUnitReady(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::Rezero(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::RequestSense(SASIDEV *controller)
{
	int lun = controller->GetEffectiveLun();

    // Note: According to the SCSI specs the LUN handling for REQUEST SENSE non-existing LUNs do *not* result
	// in CHECK CONDITION. Only the Sense Key and ASC are set in order to signal the non-existing LUN.
	if (!ctrl->unit[lun]) {
        // LUN 0 can be assumed to be present (required to call RequestSense() below)
		lun = 0;

		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_LUN);
		ctrl->status = 0x00;
	}

    ctrl->length = ctrl->unit[lun]->RequestSense(ctrl->cmd, ctrl->buffer);
	ASSERT(ctrl->length > 0);

    LOGTRACE("%s Status $%02X, Sense Key $%02X, ASC $%02X",__PRETTY_FUNCTION__, ctrl->status, ctrl->buffer[2], ctrl->buffer[12]);

    controller->DataIn();
}

void Disk::FormatUnit(SASIDEV *controller)
{
	if (!Format(ctrl->cmd)) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::ReassignBlocks(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::Read(SASIDEV *controller, uint64_t record)
{
	ctrl->length = Read(ctrl->cmd, ctrl->buffer, record);
	LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, (int)ctrl->length);

	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	// Set next block
	ctrl->next = record + 1;

	controller->DataIn();
}

void Disk::Read6(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW6)) {
		LOGDEBUG("%s READ(6) command record=$%08X blocks=%d", __PRETTY_FUNCTION__, (uint32_t)start, ctrl->blocks);

		Read(controller, start);
	}
}

void Disk::Read10(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW10)) {
		LOGDEBUG("%s READ(10) command record=$%08X blocks=%d", __PRETTY_FUNCTION__, (uint32_t)start, ctrl->blocks);

		Read(controller, start);
	}
}

void Disk::Read16(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW16)) {
		LOGDEBUG("%s READ(16) command record=$%08X blocks=%d", __PRETTY_FUNCTION__, (uint32_t)start, ctrl->blocks);

		Read(controller, start);
	}
}

void Disk::ReadWriteLong10(SASIDEV *controller)
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (ctrl->cmd[7] || ctrl->cmd[8]) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		return;
	}

	if (CheckBlockAddress(controller, RW10)) {
		controller->Status();
	}
}

void Disk::ReadLong10(SASIDEV *controller)
{
	ReadWriteLong10(controller);
}

void Disk::ReadWriteLong16(SASIDEV *controller)
{
	// Transfer lengths other than 0 are not supported, which is compliant with the SCSI standard
	if (ctrl->cmd[12] || ctrl->cmd[13]) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		return;
	}

	if (CheckBlockAddress(controller, RW16)) {
		controller->Status();
	}
}

void Disk::ReadLong16(SASIDEV *controller)
{
	ReadWriteLong16(controller);
}

void Disk::Write(SASIDEV *controller, uint64_t record)
{
	ctrl->length = WriteCheck(record);
	if (ctrl->length == 0) {
		controller->Error(ERROR_CODES::sense_key::NOT_READY, ERROR_CODES::asc::NO_ADDITIONAL_SENSE_INFORMATION);
		return;
	}
	else if (ctrl->length < 0) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::WRITE_PROTECTED);
		return;
	}

	// Set next block
	ctrl->next = record + 1;

	controller->DataOut();
}

void Disk::Write6(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW6)) {
		LOGDEBUG("%s WRITE(6) command record=$%08X blocks=%d", __PRETTY_FUNCTION__, (uint32_t)start, ctrl->blocks);

		Write(controller, start);
	}
}

void Disk::Write10(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW10)) {
		LOGDEBUG("%s WRITE(10) command record=$%08X blocks=%d",__PRETTY_FUNCTION__, (uint32_t)start, ctrl->blocks);

		Write(controller, start);
	}
}

void Disk::Write16(SASIDEV *controller)
{
	uint64_t start;
	if (GetStartAndCount(controller, start, ctrl->blocks, RW16)) {
		LOGDEBUG("%s WRITE(16) command record=$%08X blocks=%d",__PRETTY_FUNCTION__, (uint32_t)start, ctrl->blocks);

		Write(controller, start);
	}
}

void Disk::WriteLong10(SASIDEV *controller)
{
	ReadWriteLong10(controller);
}

void Disk::WriteLong16(SASIDEV *controller)
{
	ReadWriteLong16(controller);
}

void Disk::Verify(SASIDEV *controller, uint64_t record)
{
	// if BytChk=0
	if ((ctrl->cmd[1] & 0x02) == 0) {
		Seek(controller);
		return;
	}

	// Test loading
	ctrl->length = Read(ctrl->cmd, ctrl->buffer, record);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	// Set next block
	ctrl->next = record + 1;

	controller->DataOut();
}

void Disk::Verify10(SASIDEV *controller)
{
	// Get record number and block number
	uint64_t record;
	if (GetStartAndCount(controller, record, ctrl->blocks, RW10)) {
		LOGDEBUG("%s VERIFY(10) command record=$%08X blocks=%d",__PRETTY_FUNCTION__, (uint32_t)record, ctrl->blocks);

		Verify(controller, record);
	}
}

void Disk::Verify16(SASIDEV *controller)
{
	// Get record number and block number
	uint64_t record;
	if (GetStartAndCount(controller, record, ctrl->blocks, RW16)) {
		LOGDEBUG("%s VERIFY(16) command record=$%08X blocks=%d",__PRETTY_FUNCTION__, (uint32_t)record, ctrl->blocks);

		Verify(controller, record);
	}
}

void Disk::Inquiry(SASIDEV *controller)
{
	int lun = controller->GetEffectiveLun();
	const ScsiPrimaryCommands *device = ctrl->unit[lun];

	// Find a valid unit
	// TODO The code below is probably wrong. It results in the same INQUIRY data being
	// used for all LUNs, even though each LUN has its individual set of INQUIRY data.
	// In addition, it supports gaps in the LUN list, which is not correct.
	if (!device) {
		for (int valid_lun = 0; valid_lun < SASIDEV::UnitMax; valid_lun++) {
			if (ctrl->unit[valid_lun]) {
				device = ctrl->unit[valid_lun];
				break;
			}
		}
	}

	if (device) {
		ctrl->length = Inquiry(ctrl->cmd, ctrl->buffer);
	} else {
		ctrl->length = 0;
	}

	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	// Report if the device does not support the requested LUN
	if (!ctrl->unit[lun]) {
		LOGDEBUG("Reporting LUN %d for device ID %d as not supported", lun, ctrl->device->GetId());

		ctrl->buffer[0] |= 0x7f;
	}

	controller->DataIn();
}

void Disk::ModeSelect6(SASIDEV *controller)
{
	LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, ctrl->buffer[0]);

	ctrl->length = ModeSelectCheck6(ctrl->cmd);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataOut();
}

void Disk::ModeSelect10(SASIDEV *controller)
{
	LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, ctrl->buffer[0]);

	ctrl->length = ModeSelectCheck10(ctrl->cmd);
	if (ctrl->length <= 0) {
		controller->Error();
		return;
	}

	controller->DataOut();
}

void Disk::ModeSense6(SASIDEV *controller)
{
	ctrl->length = ModeSense6(ctrl->cmd, ctrl->buffer);
	if (ctrl->length <= 0) {
		LOGTRACE("%s Unsupported mode page $%02X",__PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[2]);

		controller->Error();
		return;
	}

	controller->DataIn();
}

void Disk::ModeSense10(SASIDEV *controller)
{
	ctrl->length = ModeSense10(ctrl->cmd, ctrl->buffer);
	if (ctrl->length <= 0) {
		LOGTRACE("%s Unsupported mode page $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[2]);

		controller->Error();
		return;
	}

	controller->DataIn();
}

void Disk::StartStopUnit(SASIDEV *controller)
{
	if (!StartStop(ctrl->cmd)) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::SendDiagnostic(SASIDEV *controller)
{
	if (!SendDiag(ctrl->cmd)) {
		controller->Error();
		return;
	}

	controller->Status();
}

void Disk::PreventAllowMediumRemoval(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	bool lock = ctrl->cmd[4] & 0x01;

	LOGTRACE("%s", lock ? "Locking medium" : "Unlocking medium");

	SetLocked(lock);

	controller->Status();
}

void Disk::SynchronizeCache10(SASIDEV *controller)
{
	// Nothing to do

	controller->Status();
}

void Disk::SynchronizeCache16(SASIDEV *controller)
{
	return SynchronizeCache10(controller);
}

void Disk::ReadDefectData10(SASIDEV *controller)
{
	ctrl->length = ReadDefectData10(ctrl->cmd, ctrl->buffer);
	if (ctrl->length <= 4) {
		controller->Error();
		return;
	}

	controller->DataIn();
}

bool Disk::Eject(bool force)
{
	bool status = Device::Eject(force);
	if (status) {
		// Remove disk cache
		disk.dcache->Save();
		delete disk.dcache;
		disk.dcache = NULL;

		// The image file for this drive is not in use anymore
		FileSupport *file_support = dynamic_cast<FileSupport *>(this);
		if (file_support) {
			file_support->UnreserveFile();
		}
	}

	return status;
}

bool Disk::CheckReady()
{
	// Not ready if reset
	if (IsReset()) {
		SetStatusCode(STATUS_DEVRESET);
		SetReset(false);
		LOGTRACE("%s Disk in reset", __PRETTY_FUNCTION__);
		return false;
	}

	// Not ready if it needs attention
	if (IsAttn()) {
		SetStatusCode(STATUS_ATTENTION);
		SetAttn(false);
		LOGTRACE("%s Disk in needs attention", __PRETTY_FUNCTION__);
		return false;
	}

	// Return status if not ready
	if (!IsReady()) {
		SetStatusCode(STATUS_NOTREADY);
		LOGTRACE("%s Disk not ready", __PRETTY_FUNCTION__);
		return false;
	}

	// Initialization with no error
	LOGTRACE("%s Disk is ready", __PRETTY_FUNCTION__);

	return true;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//	*SASI is a separate process
//
//---------------------------------------------------------------------------
int Disk::RequestSense(const DWORD *cdb, BYTE *buf)
{
	ASSERT(cdb);
	ASSERT(buf);

	// Return not ready only if there are no errors
	if (GetStatusCode() == STATUS_NOERROR) {
		if (!IsReady()) {
			SetStatusCode(STATUS_NOTREADY);
		}
	}

	// Size determination (according to allocation length)
	int size = (int)cdb[4];
	ASSERT((size >= 0) && (size < 0x100));

	// For SCSI-1, transfer 4 bytes when the size is 0
    // (Deleted this specification for SCSI-2)
	if (size == 0) {
		size = 4;
	}

	// Clear the buffer
	memset(buf, 0, size);

	// Set 18 bytes including extended sense data

	// Current error
	buf[0] = 0x70;

	buf[2] = (BYTE)(GetStatusCode() >> 16);
	buf[7] = 10;
	buf[12] = (BYTE)(GetStatusCode() >> 8);
	buf[13] = (BYTE)GetStatusCode();

	return size;
}

int Disk::ModeSelectCheck(const DWORD *cdb, int length)
{
	// Error if save parameters are set for other types than of SCHD or SCRM
	if (!IsSCSIHD() && (cdb[1] & 0x01)) {
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}

	return length;
}

int Disk::ModeSelectCheck6(const DWORD *cdb)
{
	// Receive the data specified by the parameter length
	return ModeSelectCheck(cdb, cdb[4]);
}

int Disk::ModeSelectCheck10(const DWORD *cdb)
{
	// Receive the data specified by the parameter length
	int length = cdb[7];
	length <<= 8;
	length |= cdb[8];
	if (length > 0x800) {
		length = 0x800;
	}

	return ModeSelectCheck(cdb, length);
}

bool Disk::ModeSelect(const DWORD* /*cdb*/, const BYTE *buf, int length)
{
	ASSERT(buf);
	ASSERT(length >= 0);

	// cannot be set
	SetStatusCode(STATUS_INVALIDPRM);

	return false;
}

int Disk::ModeSense6(const DWORD *cdb, BYTE *buf)
{
	// Get length, clear buffer
	int length = (int)cdb[4];
	ASSERT((length >= 0) && (length < 0x100));
	memset(buf, 0, length);

	// Get changeable flag
	bool change = (cdb[2] & 0xc0) == 0x40;

	// Get page code (0x00 is valid from the beginning)
	int page = cdb[2] & 0x3f;
	bool valid = page == 0x00;

	// Basic information
	int size = 4;

	// MEDIUM TYPE
	if (IsMo()) {
		buf[1] = 0x03; // optical reversible or erasable
	}

	// DEVICE SPECIFIC PARAMETER
	if (IsProtected()) {
		buf[2] = 0x80;
	}

	// add block descriptor if DBD is 0
	if ((cdb[1] & 0x08) == 0) {
		// Mode parameter header, block descriptor length
		buf[3] = 0x08;

		// Only if ready
		if (IsReady()) {
			// Short LBA mode parameter block descriptor (number of blocks and block length)

			uint64_t disk_blocks = GetBlockCount();
			buf[4] = disk_blocks >> 24;
			buf[5] = disk_blocks >> 16;
			buf[6] = disk_blocks >> 8;
			buf[7] = disk_blocks;

			// Block descriptor (block length)
			uint32_t disk_size = GetSectorSizeInBytes();
			buf[9] = disk_size >> 16;
			buf[10] = disk_size >> 8;
			buf[11] = disk_size;
		}

		// size
		size = 12;
	}

	// Page code 1(read-write error recovery)
	if ((page == 0x01) || (page == 0x3f)) {
		size += AddErrorPage(change, &buf[size]);
		valid = true;
	}

	// Page code 3(format device)
	if ((page == 0x03) || (page == 0x3f)) {
		size += AddFormatPage(change, &buf[size]);
		valid = true;
	}

	// Page code 4(drive parameter)
	if ((page == 0x04) || (page == 0x3f)) {
		size += AddDrivePage(change, &buf[size]);
		valid = true;
	}

	// Page code 6(optical)
	if (IsMo()) {
		if ((page == 0x06) || (page == 0x3f)) {
			size += AddOptionPage(change, &buf[size]);
			valid = true;
		}
	}

	// Page code 8(caching)
	if ((page == 0x08) || (page == 0x3f)) {
		size += AddCachePage(change, &buf[size]);
		valid = true;
	}

	// Page code 13(CD-ROM)
	if (IsCdRom()) {
		if ((page == 0x0d) || (page == 0x3f)) {
			size += AddCDROMPage(change, &buf[size]);
			valid = true;
		}
	}

	// Page code 14(CD-DA)
	if (IsCdRom()) {
		if ((page == 0x0e) || (page == 0x3f)) {
			size += AddCDDAPage(change, &buf[size]);
			valid = true;
		}
	}

	// Page (vendor special)
	int ret = AddVendorPage(page, change, &buf[size]);
	if (ret > 0) {
		size += ret;
		valid = true;
	}

	// final setting of mode data length
	buf[0] = size - 1;

	// Unsupported page
	if (!valid) {
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}
	//check if size of data is more than size requested.
	if (size > length) {
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}
	//Set length returned to actual size of data
	length = size;

	return length;
}

int Disk::ModeSense10(const DWORD *cdb, BYTE *buf)
{
	// Get length, clear buffer
	int length = cdb[7];
	length <<= 8;
	length |= cdb[8];
	if (length > 0x800) {
		length = 0x800;
	}
	ASSERT((length >= 0) && (length < 0x800));
	memset(buf, 0, length);

	// Get changeable flag
	bool change = (cdb[2] & 0xc0) == 0x40;

	// Get page code (0x00 is valid from the beginning)
	int page = cdb[2] & 0x3f;
	bool valid = page == 0x00;

	// Basic Information
	int size = 8;

	// MEDIUM TYPE
	if (IsMo()) {
		buf[2] = 0x03; // optical reversible or erasable
	}

	// DEVICE SPECIFIC PARAMETER
	if (IsProtected()) {
		buf[3] = 0x80;
	}

	// add block descriptor if DBD is 0
	if ((cdb[1] & 0x08) == 0) {
		// Only if ready
		if (IsReady()) {
			uint64_t disk_blocks = GetBlockCount();
			uint32_t disk_size = GetSectorSizeInBytes();

			// Check LLBAA for short or long block descriptor
			if ((cdb[1] & 0x10) == 0 || disk_blocks <= 0xFFFFFFFF) {
				// Mode parameter header, block descriptor length
				buf[7] = 0x08;

				// Short LBA mode parameter block descriptor (number of blocks and block length)

				buf[8] = disk_blocks >> 24;
				buf[9] = disk_blocks >> 16;
				buf[10] = disk_blocks >> 8;
				buf[11] = disk_blocks;

				buf[13] = disk_size >> 16;
				buf[14] = disk_size >> 8;
				buf[15] = disk_size;

				size = 16;
			}
			else {
				// Mode parameter header, LONGLBA
				buf[4] = 0x01;

				// Mode parameter header, block descriptor length
				buf[7] = 0x10;

				// Long LBA mode parameter block descriptor (number of blocks and block length)

				buf[8] = disk_blocks >> 56;
				buf[9] = disk_blocks >> 48;
				buf[10] = disk_blocks >> 40;
				buf[11] = disk_blocks >> 32;
				buf[12] = disk_blocks >> 24;
				buf[13] = disk_blocks >> 16;
				buf[14] = disk_blocks >> 8;
				buf[15] = disk_blocks;

				buf[20] = disk_size >> 24;
				buf[21] = disk_size >> 16;
				buf[22] = disk_size >> 8;
				buf[23] = disk_size;

				size = 24;
			}
		}
	}

	// Page code 1(read-write error recovery)
	if ((page == 0x01) || (page == 0x3f)) {
		size += AddErrorPage(change, &buf[size]);
		valid = true;
	}

	// Page code 3(format device)
	if ((page == 0x03) || (page == 0x3f)) {
		size += AddFormatPage(change, &buf[size]);
		valid = true;
	}

	// Page code 4(drive parameter)
	if ((page == 0x04) || (page == 0x3f)) {
		size += AddDrivePage(change, &buf[size]);
		valid = true;
	}

	// ペPage code 6(optical)
	if (IsMo()) {
		if ((page == 0x06) || (page == 0x3f)) {
			size += AddOptionPage(change, &buf[size]);
			valid = true;
		}
	}

	// Page code 8(caching)
	if ((page == 0x08) || (page == 0x3f)) {
		size += AddCachePage(change, &buf[size]);
		valid = true;
	}

	// Page code 13(CD-ROM)
	if (IsCdRom()) {
		if ((page == 0x0d) || (page == 0x3f)) {
			size += AddCDROMPage(change, &buf[size]);
			valid = true;
		}
	}

	// Page code 14(CD-DA)
	if (IsCdRom()) {
		if ((page == 0x0e) || (page == 0x3f)) {
			size += AddCDDAPage(change, &buf[size]);
			valid = true;
		}
	}

	// Page (vendor special)
	int ret = AddVendorPage(page, change, &buf[size]);
	if (ret > 0) {
		size += ret;
		valid = true;
	}

	// final setting of mode data length
	buf[0] = (size - 2) >> 8;
	buf[1] = size - 2;

	// Unsupported page
	if (!valid) {
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}
	//check if size of data is more than size requested.
	if (size > length) {
		SetStatusCode(STATUS_INVALIDCDB);
		return 0;
	}
	//Set length returned to actual size of data
	length = size;

	return length;
}

int Disk::AddErrorPage(bool change, BYTE *buf)
{
	// Set the message length
	buf[0] = 0x01;
	buf[1] = 0x0a;

	// Retry count is 0, limit time uses internal default value
	return 12;
}

int Disk::AddFormatPage(bool change, BYTE *buf)
{
	// Set the message length
	buf[0] = 0x80 | 0x03;
	buf[1] = 0x16;

	// Show the number of bytes in the physical sector as changeable
	// (though it cannot be changed in practice)
	if (change) {
		buf[0xc] = 0xff;
		buf[0xd] = 0xff;
		return 24;
	}

	if (IsReady()) {
		// Set the number of tracks in one zone to 8 (TODO)
		buf[0x3] = 0x08;

		// Set sector/track to 25 (TODO)
		buf[0xa] = 0x00;
		buf[0xb] = 0x19;

		// Set the number of bytes in the physical sector
		int size = 1 << disk.size;
		buf[0xc] = (BYTE)(size >> 8);
		buf[0xd] = (BYTE)size;
	}

	// Set removable attribute
	if (IsRemovable()) {
		buf[20] = 0x20;
	}

	return 24;
}

int Disk::AddDrivePage(bool change, BYTE *buf)
{
	// Set the message length
	buf[0] = 0x04;
	buf[1] = 0x16;

	// No changeable area
	if (change) {
		return 24;
	}

	if (IsReady()) {
		// Set the number of cylinders (total number of blocks
        // divided by 25 sectors/track and 8 heads)
		uint32_t cylinder = disk.blocks;
		cylinder >>= 3;
		cylinder /= 25;
		buf[0x2] = (BYTE)(cylinder >> 16);
		buf[0x3] = (BYTE)(cylinder >> 8);
		buf[0x4] = (BYTE)cylinder;

		// Fix the head at 8
		buf[0x5] = 0x8;
	}

	return 24;
}

int Disk::AddOptionPage(bool change, BYTE *buf)
{
	// Set the message length
	buf[0] = 0x06;
	buf[1] = 0x02;

	// Do not report update blocks
	return 4;
}

int Disk::AddCachePage(bool change, BYTE *buf)
{
	// Set the message length
	buf[0] = 0x08;
	buf[1] = 0x0a;

	// Only read cache is valid, no prefetch
	return 12;
}

int Disk::AddCDROMPage(bool change, BYTE *buf)
{
	// Set the message length
	buf[0] = 0x0d;
	buf[1] = 0x06;

	// No changeable area
	if (change) {
		return 8;
	}

	// 2 seconds for inactive timer
	buf[3] = 0x05;

	// MSF multiples are 60 and 75 respectively
	buf[5] = 60;
	buf[7] = 75;

	return 8;
}

int Disk::AddCDDAPage(bool change, BYTE *buf)
{
	// Set the message length
	buf[0] = 0x0e;
	buf[1] = 0x0e;

	// Audio waits for operation completion and allows
	// PLAY across multiple tracks
	return 16;
}

int Disk::AddVendorPage(int /*page*/, bool /*change*/, BYTE *buf)
{
	ASSERT(buf);

	return 0;
}

int Disk::ReadDefectData10(const DWORD *cdb, BYTE *buf)
{
	ASSERT(cdb);
	ASSERT(buf);

	// Get length, clear buffer
	DWORD length = cdb[7];
	length <<= 8;
	length |= cdb[8];
	if (length > 0x800) {
		length = 0x800;
	}
	ASSERT((length >= 0) && (length < 0x800));
	memset(buf, 0, length);

	// P/G/FORMAT
	buf[1] = (cdb[1] & 0x18) | 5;
	buf[3] = 8;

	buf[4] = 0xff;
	buf[5] = 0xff;
	buf[6] = 0xff;
	buf[7] = 0xff;

	buf[8] = 0xff;
	buf[9] = 0xff;
	buf[10] = 0xff;
	buf[11] = 0xff;

	// no list
	SetStatusCode(STATUS_NODEFECT);
	return 4;
}

//---------------------------------------------------------------------------
//
//	FORMAT UNIT
//	*Opcode $06 for SASI, Opcode $04 for SCSI
//
//---------------------------------------------------------------------------
bool Disk::Format(const DWORD *cdb)
{
	if (!CheckReady()) {
		return false;
	}

	// FMTDATA=1 is not supported (but OK if there is no DEFECT LIST)
	if ((cdb[1] & 0x10) != 0 && cdb[4] != 0) {
		SetStatusCode(STATUS_INVALIDCDB);
		return false;
	}

	// FORMAT Success
	return true;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
// TODO Read more than one block in a single call. Currently blocked by the SASI code (missing early range check)
// and the track-oriented cache.
int Disk::Read(const DWORD *cdb, BYTE *buf, uint64_t block)
{
	ASSERT(buf);

	LOGTRACE("%s", __PRETTY_FUNCTION__);

	if (!CheckReady()) {
		return 0;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		SetStatusCode(STATUS_INVALIDLBA);
		return 0;
	}

	// leave it to the cache
	if (!disk.dcache->ReadSector(buf, block)) {
		SetStatusCode(STATUS_READFAULT);
		return 0;
	}

	//  Success
	return (1 << disk.size);
}

//---------------------------------------------------------------------------
//
//	WRITE check
//
//---------------------------------------------------------------------------
int Disk::WriteCheck(DWORD block)
{
	// Status check
	if (!CheckReady()) {
		LOGDEBUG("WriteCheck failed (not ready)");
		return 0;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		LOGDEBUG("WriteCheck failed (capacity exceeded)");
		return 0;
	}

	// Error if write protected
	if (IsProtected()) {
		LOGDEBUG("WriteCheck failed (protected)");
		return -1;
	}

	//  Success
	return (1 << disk.size);
}

//---------------------------------------------------------------------------
//
//	WRITE
//
//---------------------------------------------------------------------------
// TODO Write more than one block in a single call. Currently blocked by the SASI code (missing early range check)
// and the track-oriented cache.
bool Disk::Write(const DWORD *cdb, const BYTE *buf, DWORD block)
{
	ASSERT(buf);

	LOGTRACE("%s", __PRETTY_FUNCTION__);

	// Error if not ready
	if (!IsReady()) {
		SetStatusCode(STATUS_NOTREADY);
		return false;
	}

	// Error if the total number of blocks is exceeded
	if (block >= disk.blocks) {
		SetStatusCode(STATUS_INVALIDLBA);
		return false;
	}

	// Error if write protected
	if (IsProtected()) {
		SetStatusCode(STATUS_WRITEPROTECT);
		return false;
	}

	// Leave it to the cache
	if (!disk.dcache->WriteSector(buf, block)) {
		SetStatusCode(STATUS_WRITEFAULT);
		return false;
	}

	return true;
}

void Disk::Seek(SASIDEV *controller)
{
	if (!CheckReady()) {
		controller->Error();
		return;
	}

	controller->Status();
}

//---------------------------------------------------------------------------
//
//	SEEK(6)
//	Does not check LBA (SASI IOCS)
//
//---------------------------------------------------------------------------
void Disk::Seek6(SASIDEV *controller)
{
	Seek(controller);
}

void Disk::Seek10(SASIDEV *controller)
{
	Seek(controller);
}

bool Disk::StartStop(const DWORD *cdb)
{
	bool start = cdb[4] & 0x01;
	bool load = cdb[4] & 0x02;

	if (load) {
		LOGTRACE("%s", start ? "Loading medium" : "Ejecting medium");
	}
	else {
		LOGTRACE("%s", start ? "Starting unit" : "Stopping unit");

		SetStopped(!start);
	}

	// Look at the eject bit and eject if necessary
	if (load && !start) {
		if (IsLocked()) {
			// Cannot be ejected because it is locked
			SetStatusCode(STATUS_PREVENT);
			return false;
		}

		// Eject
		return Eject(false);
	}

	return true;
}

bool Disk::SendDiag(const DWORD *cdb)
{
	// Do not support PF bit
	if (cdb[1] & 0x10) {
		SetStatusCode(STATUS_INVALIDCDB);
		return false;
	}

	// Do not support parameter list
	if ((cdb[3] != 0) || (cdb[4] != 0)) {
		SetStatusCode(STATUS_INVALIDCDB);
		return false;
	}

	return true;
}

void Disk::ReadCapacity10(SASIDEV *controller)
{
	BYTE *buf = ctrl->buffer;

	memset(buf, 0, 8);

	if (!CheckReady()) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::MEDIUM_NOT_PRESENT);
		return;
	}

	if (disk.blocks <= 0) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::MEDIUM_NOT_PRESENT);

		LOGWARN("%s Capacity not available, medium may not be present", __PRETTY_FUNCTION__);

		return;
	}

	// Create end of logical block address (disk.blocks-1)
	uint32_t blocks = disk.blocks - 1;
	buf[0] = (BYTE)(blocks >> 24);
	buf[1] = (BYTE)(blocks >> 16);
	buf[2] = (BYTE)(blocks >> 8);
	buf[3] = (BYTE)blocks;

	// Create block length (1 << disk.size)
	uint32_t length = 1 << disk.size;
	buf[4] = (BYTE)(length >> 24);
	buf[5] = (BYTE)(length >> 16);
	buf[6] = (BYTE)(length >> 8);
	buf[7] = (BYTE)length;

	// the size
	ctrl->length = 8;

	controller->DataIn();
}

void Disk::ReadCapacity16(SASIDEV *controller)
{
	BYTE *buf = ctrl->buffer;

	memset(buf, 0, 14);

	if (!CheckReady() || disk.blocks <= 0) {
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::MEDIUM_NOT_PRESENT);
		return;
	}

	// Create end of logical block address (disk.blocks-1)
	uint64_t blocks = disk.blocks - 1;
	buf[0] = (BYTE)(blocks >> 56);
	buf[1] = (BYTE)(blocks >> 48);
	buf[2] = (BYTE)(blocks >> 40);
	buf[3] = (BYTE)(blocks >> 32);
	buf[4] = (BYTE)(blocks >> 24);
	buf[5] = (BYTE)(blocks >> 16);
	buf[6] = (BYTE)(blocks >> 8);
	buf[7] = (BYTE)blocks;

	// Create block length (1 << disk.size)
	uint32_t length = 1 << disk.size;
	buf[8] = (BYTE)(length >> 24);
	buf[9] = (BYTE)(length >> 16);
	buf[10] = (BYTE)(length >> 8);
	buf[11] = (BYTE)length;

	// Logical blocks per physical block: not reported (1 or more)
	buf[13] = 0;

	// the size
	ctrl->length = 14;

	controller->DataIn();
}

void Disk::ReadCapacity16_ReadLong16(SASIDEV *controller)
{
	// The service action determines the actual command
	switch (ctrl->cmd[1] & 0x1f) {
	case 0x10:
		ReadCapacity16(controller);
		break;

	case 0x11:
		ReadLong16(controller);
		break;

	default:
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		break;
	}
}

void Disk::ReportLuns(SASIDEV *controller)
{
	BYTE *buf = ctrl->buffer;

	if (!CheckReady()) {
		controller->Error();
		return;
	}

	int allocation_length = (ctrl->cmd[6] << 24) + (ctrl->cmd[7] << 16) + (ctrl->cmd[8] << 8) + ctrl->cmd[9];
	memset(buf, 0, allocation_length);

	// Count number of available LUNs for the current device
	int luns;
	for (luns = 0; luns < controller->GetCtrl()->device->GetSupportedLuns(); luns++) {
		if (!controller->GetCtrl()->unit[luns]) {
			break;
		}
	}

	// LUN list length, 8 bytes per LUN
	// SCSI standard: The contents of the LUN LIST LENGTH field	are not altered based on the allocation length
	buf[0] = (luns * 8) >> 24;
	buf[1] = (luns * 8) >> 16;
	buf[2] = (luns * 8) >> 8;
	buf[3] = luns * 8;

	ctrl->length = allocation_length < 8 + luns * 8 ? allocation_length : 8 + luns * 8;

	controller->DataIn();
}

//---------------------------------------------------------------------------
//
//	RESERVE(6)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void Disk::Reserve6(SASIDEV *controller)
{
	controller->Status();
}

//---------------------------------------------------------------------------
//
//	RESERVE(10)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void Disk::Reserve10(SASIDEV *controller)
{
	controller->Status();
}

//---------------------------------------------------------------------------
//
//	RELEASE(6)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void Disk::Release6(SASIDEV *controller)
{
	controller->Status();
}

//---------------------------------------------------------------------------
//
//	RELEASE(10)
//
//  The reserve/release commands are only used in multi-initiator
//  environments. RaSCSI doesn't support this use case. However, some old
//  versions of Solaris will issue the reserve/release commands. We will
//  just respond with an OK status.
//
//---------------------------------------------------------------------------
void Disk::Release10(SASIDEV *controller)
{
	controller->Status();
}

//---------------------------------------------------------------------------
//
//	Check/Get start sector and sector count for a READ/WRITE or READ/WRITE LONG operation
//
//---------------------------------------------------------------------------

bool Disk::CheckBlockAddress(SASIDEV *controller, access_mode mode)
{
	uint64_t block = ctrl->cmd[2];
	block <<= 8;
	block |= ctrl->cmd[3];
	block <<= 8;
	block |= ctrl->cmd[4];
	block <<= 8;
	block |= ctrl->cmd[5];

	if (mode == RW16) {
		block <<= 8;
		block |= ctrl->cmd[6];
		block <<= 8;
		block |= ctrl->cmd[7];
		block <<= 8;
		block |= ctrl->cmd[8];
		block <<= 8;
		block |= ctrl->cmd[9];
	}

	uint64_t capacity = GetBlockCount();
	if (block > capacity) {
		ostringstream s;
		s << "Capacity of " << capacity << " blocks exceeded: " << "Trying to access block " << block;
		LOGTRACE("%s", s.str().c_str());
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return false;
	}

	return true;
}

bool Disk::GetStartAndCount(SASIDEV *controller, uint64_t& start, uint32_t& count, access_mode mode)
{
	if (mode == RW6) {
		start = ctrl->cmd[1] & 0x1f;
		start <<= 8;
		start |= ctrl->cmd[2];
		start <<= 8;
		start |= ctrl->cmd[3];

		count = ctrl->cmd[4];
		if (!count) {
			count= 0x100;
		}
	}
	else {
		start = ctrl->cmd[2];
		start <<= 8;
		start |= ctrl->cmd[3];
		start <<= 8;
		start |= ctrl->cmd[4];
		start <<= 8;
		start |= ctrl->cmd[5];

		if (mode == RW16) {
			start <<= 8;
			start |= ctrl->cmd[6];
			start <<= 8;
			start |= ctrl->cmd[7];
			start <<= 8;
			start |= ctrl->cmd[8];
			start <<= 8;
			start |= ctrl->cmd[9];
		}

		if (mode == RW16) {
			count = ctrl->cmd[10];
			count <<= 8;
			count |= ctrl->cmd[11];
			count <<= 8;
			count |= ctrl->cmd[12];
			count <<= 8;
			count |= ctrl->cmd[13];
		}
		else {
			count = ctrl->cmd[7];
			count <<= 8;
			count |= ctrl->cmd[8];
		}
	}

	// Check capacity
	uint64_t capacity = GetBlockCount();
	if (start > capacity || start + count > capacity) {
		ostringstream s;
		s << "Capacity of " << capacity << " blocks exceeded: "
				<< "Trying to read block " << start << ", block count " << ctrl->blocks;
		LOGTRACE("%s", s.str().c_str());
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::LBA_OUT_OF_RANGE);
		return false;
	}

	// Do not process 0 blocks
	if (!count) {
		LOGTRACE("NOT processing 0 blocks");
		controller->Status();
		return false;
	}

	return true;
}

uint32_t Disk::GetSectorSizeInBytes() const
{
	return disk.size ? 1 << disk.size : 0;
}

void Disk::SetSectorSizeInBytes(uint32_t size, bool sasi)
{
	set<uint32_t> sector_sizes = DeviceFactory::instance().GetSectorSizes(GetType());
	if (!sector_sizes.empty() && sector_sizes.find(size) == sector_sizes.end()) {
		stringstream error;
		error << "Invalid block size of " << size << " bytes";
		throw io_exception(error.str());
	}

	switch (size) {
		case 256:
			disk.size = 8;
			break;

		case 512:
			disk.size = 9;
			break;

		case 1024:
			disk.size = 10;
			break;

		case 2048:
			disk.size = 11;
			break;

		case 4096:
			disk.size = 12;
			break;

		default:
			assert(false);
			break;
	}
}

uint32_t Disk::GetSectorSizeShiftCount() const
{
	return disk.size;
}

void Disk::SetSectorSizeShiftCount(uint32_t size)
{
	disk.size = size;
}

bool Disk::IsSectorSizeConfigurable() const
{
	return !sector_sizes.empty();
}

void Disk::SetSectorSizes(const set<uint32_t>& sector_sizes)
{
	this->sector_sizes = sector_sizes;
}

uint32_t Disk::GetConfiguredSectorSize() const
{
	return configured_sector_size;
}

bool Disk::SetConfiguredSectorSize(uint32_t configured_sector_size)
{
	DeviceFactory& device_factory = DeviceFactory::instance();

	set<uint32_t> sector_sizes = device_factory.GetSectorSizes(GetType());
	if (sector_sizes.find(configured_sector_size) == sector_sizes.end()) {
		return false;
	}

	this->configured_sector_size = configured_sector_size;

	return true;
}

void Disk::SetGeometries(const map<uint64_t, Geometry>& geometries)
{
	this->geometries = geometries;
}

bool Disk::SetGeometryForCapacity(uint64_t capacity) {
	const auto& geometry = geometries.find(capacity);
	if (geometry != geometries.end()) {
		SetSectorSizeInBytes(geometry->second.first, false);
		SetBlockCount(geometry->second.second);

		return true;
	}

	return false;
}

uint64_t Disk::GetBlockCount() const
{
	return disk.blocks;
}

void Disk::SetBlockCount(uint32_t blocks)
{
	disk.blocks = blocks;
}
