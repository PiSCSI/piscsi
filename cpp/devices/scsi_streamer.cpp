//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2024 BogDan Vatra <bogdan@kde.org>
//
//---------------------------------------------------------------------------

#include "scsi_streamer.h"

#include "scsi_command_util.h"

using namespace scsi_command_util;


File::~File()
{
	close();
}

bool File::open(string_view filename)
{
	close();
	file = fopen(filename.data(), "r+");
	if (!file)
		throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
	return file != nullptr;
}

void File::rewind()
{
	seek(0, SEEK_SET);
}

size_t File::read(uint8_t *buff, size_t size, size_t count)
{
	if (!file)
		throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
	return checkSize(fread(buff, size, count, file));
}

size_t File::write(const uint8_t* buff, size_t size, size_t count)
{
	if (!file)
		throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
	return checkSize(fwrite(buff, size, count, file));
}

void File::seek(long offset, int origin)
{
	if (!file)
		throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);
	if (fseek(file, offset, origin))
		throw scsi_exception(sense_key::medium_error);
}

long File::tell()
{
	if (!file)
		throw scsi_exception(sense_key::illegal_request, asc::medium_not_present);

	return checkSize(ftell(file));
}

void File::close()
{
	if (file) {
		fclose(file);
		file = nullptr;
	}
}


SCSIST::SCSIST(int lun)
	: StorageDevice(SCST, lun, {512})
{
	SetProtectable(true);
	SetRemovable(true);
	SetLockable(true);
	SupportsSaveParameters(true);
	SetVendor("TANDBERG"); // Masquerade as Tandberg
	SetProduct(" TDC Streamer");
}

bool SCSIST::Init(const param_map &pm)
{
	// Mandatory commands
	// | INQUIRY                             |    12h     |   M  |   8.2.5    |
	// | MODE SELECT(6)                      |    15h     |   M  |   8.2.8    |
	// | MODE SENSE(6)                       |    1Ah     |   M  |   8.2.10   |
	// | RELEASE UNIT                        |    17h     |   M  |  10.2.9    |
	// | REQUEST SENSE                       |    03h     |   M  |   8.2.14   |
	// | RESERVE UNIT                        |    16h     |   M  |  10.2.10   |
	// | SEND DIAGNOSTIC                     |    1Dh     |   M  |   8.2.15   |
	// | TEST UNIT READY                     |    00h     |   M  |   8.2.16   |

	// Optional commands
	// | MODE SELECT(10)                     |    55h     |   O  |   8.2.9    |
	// | MODE SENSE(10)                      |    5Ah     |   O  |   8.2.11   |
	// | PREVENT ALLOW MEDIUM REMOVAL        |    1Eh     |   O  |   9.2.4    |
	if (!StorageDevice::Init(pm))
		return false;

	// Mandatory commands
	// | ERASE                               |    19h     |   M  |  10.2.1    |
	AddCommand(scsi_command::eCmdErase, [this] { Erase(); });

	// | READ                                |    08h     |   M  |  10.2.4    |
	AddCommand(scsi_command::eCmdRead6, [this] { Read6(); });

	// | READ BLOCK LIMITS                   |    05h     |   M  |  10.2.5    |
	AddCommand(scsi_command::eCmdReadBlockLimits, [this] { ReadBlockLimits(); });

	// | REWIND                              |    01h     |   M  |  10.2.11   |
	AddCommand(scsi_command::eCmdRezero, [this] { Rewind(); });

	// | SPACE                               |    11h     |   M  |  10.2.12   |
	AddCommand(scsi_command::eCmdSpace, [this] { Space(); });

	// | WRITE                               |    0Ah     |   M  |  10.2.14   |
	AddCommand(scsi_command::eCmdWrite6, [this] { Write6(); });

	// | WRITE FILEMARKS                     |    10h     |   M  |  10.2.15   |
	AddCommand(scsi_command::eCmdWriteFilemarks, [this] { WriteFilemarks(); });

	// Optional commands

	// | LOAD UNLOAD                         |    1Bh     |   O  |  10.2.2    |
	AddCommand(scsi_command::eCmdStartStop, [this] { LoadUnload(); });

	// | READ POSITION                       |    34h     |   O  |  10.2.6    |
	AddCommand(scsi_command::eCmdReadPosition, [this] { ReadPosition(); });

	// | VERIFY                              |    13h     |   O  |  10.2.13   |
	AddCommand(scsi_command::eCmdVerify, [this] { Verify(); });

	/*
	| CHANGE DEFINITION                   |    40h     |   O  |   8.2.1    |
	| COMPARE                             |    39h     |   O  |   8.2.2    |
	| COPY                                |    18h     |   O  |   8.2.3    |
	| COPY AND VERIFY                     |    3Ah     |   O  |   8.2.4    |
	| LOCATE                              |    2Bh     |   O  |  10.2.3    |
	| LOG SELECT                          |    4Ch     |   O  |   8.2.6    |
	| LOG SENSE                           |    4Dh     |   O  |   8.2.7    |
	| READ BUFFER                         |    3Ch     |   O  |   8.2.12   |
	| READ REVERSE                        |    0Fh     |   O  |  10.2.7    |
	| RECEIVE DIAGNOSTIC RESULTS          |    1Ch     |   O  |   8.2.13   |
	| RECOVER BUFFERED DATA               |    14h     |   O  |  10.2.8    |
	| WRITE BUFFER                        |    3Bh     |   O  |   8.2.17   |
*/
	return true;
}

void SCSIST::CleanUp()
{
	file.close();
	StorageDevice::CleanUp();
}

std::vector<uint8_t> SCSIST::InquiryInternal() const
{
	return HandleInquiry(scsi_defs::device_type::sad, scsi_level::scsi_2, true);
}

void SCSIST::ModeSelect(scsi_command cmd, cdb_t cdb, span<const uint8_t> buff, int length)
{
	/*
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (15h)                        |
|-----+-----------------------------------------------------------------------|
| 1   | Logical unit number      |   PF   |         Reserved         |  SP    |
|-----+-----------------------------------------------------------------------|
| 2   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 3   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 4   |                           Parameter list length                       |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
*/
	if (cmd != scsi_command::eCmdModeSelect6 || length < 12)
		throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);

	// If the PF-bit is set to zero, the
	// Drive will not accept any Mode Pages in the parameter list; only the
	// Header List and a Block Descriptor List will be accepted. If the Drive
	// receives a parameter list containing bytes beyond the Header List and a
	// Block Descriptor List, it will terminate the MODE SELECT command
	// with CHECK CONDITION and set the Sense Key to ILLEGAL REQUEST and the AS/AQ sense bytes to PARAMETER LIST LENGTH
	// ERROR. The Error Code will be set to E$STE_PLEN.
	bool pf = cdb[1] & 0x10;


	// A Save Page (SP) bit of zero indicates that the Drive will perform the
	// specified MODE SELECT operation, but not save any mode parameters.
	// A SP bit of one indicates that the Drive will perform the specified
	// MODE SELECT operation and also save all saveable MODE SELECT
	// parameters received during the DATA OUT phase.
	bool sp = cdb[1] & 0x01;// this one is tricky, should we have two sets of settings ?!?!?


	// This field specifies the length in bytes of the MODE SELECT parameter
	// list that will be transferred from the Initiator to the Drive during the
	// DATA OUT phase. A Parameter List Length of zero indicates that no
	// data will be transferred. No mode selection parameters are then
	// changed. A parameter list length must never result in the truncation of
	// any header, descriptor or page of parameters.
	uint8_t params_list_len = cdb[4] & 0xff;
	if ((!pf && params_list_len != 12) || params_list_len < 12 || params_list_len > length)
		throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);

	// Header List

	// Check Block Descriptor Length
	if (buff[3] != 0x08)
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);

	// Block Descriptor List
	uint32_t block_length = GetInt24(buff, 9);
	if (sp)
		SetSectorSizeInBytes(block_length);
}

void SCSIST::Erase()
{
	/*
Table 175 - ERASE command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (19h)                        |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number    |         Reserved          | Immed |  Long  |
|-----+-----------------------------------------------------------------------|
| 2   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 3   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 4   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
*/
	file.rewind();
	EnterStatusPhase();
}

int SCSIST::ModeSense6(cdb_t cdb, std::vector<uint8_t> &buf) const
{
	/*
Table 54 - MODE SENSE(6) command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (1Ah)                        |
|-----+-----------------------------------------------------------------------|
| 1   | Logical unit number      |Reserved|   DBD  |         Reserved         |
|-----+-----------------------------------------------------------------------|
| 2   |       PC        |                   Page code                         |
|-----+-----------------------------------------------------------------------|
| 3   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 4   |                           Allocation length                           |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
A disable block descriptors (DBD) bit of zero indicates that the target may
return zero or more block descriptors in the returned MODE SENSE data
(see 8.3.3), at the target's discretion. A DBD bit of one specifies that the
target shall not return any block descriptors in the returned MODE SENSE data.
The page control (PC) field defines the type of mode parameter values to be
returned in the mode pages. The page control field is defined in table 55.


Table 55 - Page control field
+=======-=====================-============+
| Code  |  Type of parameter  | Subclause  |
|-------+---------------------+------------|
|  00b  |  Current values     |  8.2.10.1  |
|  01b  |  Changeable values  |  8.2.10.2  |
|  10b  |  Default values     |  8.2.10.3  |
|  11b  |  Saved values       |  8.2.10.4  |
+==========================================+


Table 56 - Mode page code usage for all devices
+=============-==================================================-============+
|  Page code  |  Description                                     |  Subclause |
|-------------+--------------------------------------------------+------------|
|     00h     |  Vendor-specific (does not require page format)  |            |
|  01h - 1Fh  |  See specific device-types                       |            |
|  20h - 3Eh  |  Vendor-specific (page format required)          |            |
|     3Fh     |  Return all mode pages                           |            |
+=============================================================================+
*/
	// Get length, clear buffer
	const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(cdb[4])));
	fill_n(buf.begin(), length, 0);

	// DEVICE SPECIFIC PARAMETER
	if (IsProtected()) {
		buf[2] = 0x80;
	}

	// Basic information
	int size = 4;

	// Add block descriptor if DBD is 0
	if ((cdb[1] & 0x08) == 0) {
		// Mode parameter header, block descriptor length
		buf[3] = 0x08;

		// Only if ready
		if (IsReady()) {
			// Short LBA mode parameter block descriptor (number of blocks and block length)
			SetInt32(buf, 4, static_cast<uint32_t>(GetBlockCount()));
			SetInt32(buf, 8, GetSectorSizeInBytes());
		}

		size = 12;
	}

	size = AddModePages(cdb, buf, size, length, 255);

	buf[0] = (uint8_t)size;

	return size;

}

int SCSIST::ModeSense10(cdb_t cdb, std::vector<uint8_t> &buf) const
{
	/*
Table 57 - MODE SENSE(10) command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (5Ah)                        |
|-----+-----------------------------------------------------------------------|
| 1   | Logical unit number      |Reserved|  DBD   |         Reserved         |
|-----+-----------------------------------------------------------------------|
| 2   |       PC        |                   Page code                         |
|-----+-----------------------------------------------------------------------|
| 3   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 4   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 5   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 6   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 7   | (MSB)                                                                 |
|-----+---                        Allocation length                        ---|
| 8   |                                                                 (LSB) |
|-----+-----------------------------------------------------------------------|
| 9   |                           Control                                     |
+=============================================================================+
*/
	// Get length, clear buffer
	const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(GetInt16(cdb, 7))));
	fill_n(buf.begin(), length, 0);

	// DEVICE SPECIFIC PARAMETER
	if (IsProtected()) {
		buf[3] = 0x80;
	}

	// Basic Information
	int size = 8;

	// Add block descriptor if DBD is 0, only if ready
	if ((cdb[1] & 0x08) == 0 && IsReady()) {
		uint64_t disk_blocks = GetBlockCount();
		uint32_t disk_size = GetSectorSizeInBytes();

		// Check LLBAA for short or long block descriptor
		if ((cdb[1] & 0x10) == 0 || disk_blocks <= 0xFFFFFFFF) {
			// Mode parameter header, block descriptor length
			buf[7] = 0x08;

			// Short LBA mode parameter block descriptor (number of blocks and block length)
			SetInt32(buf, 8, static_cast<uint32_t>(disk_blocks));
			SetInt32(buf, 12, disk_size);

			size = 16;
		}
		else {
			// Mode parameter header, LONGLBA
			buf[4] = 0x01;

			// Mode parameter header, block descriptor length
			buf[7] = 0x10;

			// Long LBA mode parameter block descriptor (number of blocks and block length)
			SetInt64(buf, 8, disk_blocks);
			SetInt32(buf, 20, disk_size);

			size = 24;
		}
	}

	size = AddModePages(cdb, buf, size, length, 65535);

	SetInt16(buf, 0, size);

	return size;
}

void SCSIST::Read6()
{
	/*
Table 178 - READ command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (08h)                        |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number     |        Reserved         |  SILI  | Fixed  |
|-----+-----------------------------------------------------------------------|
| 2   | (MSB)                                                                 |
|-----+---                                                                 ---|
| 3   |                           Transfer length                             |
|-----+---                                                                 ---|
| 4   |                                                                 (LSB) |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
*/
	CheckReady();
	const auto &cmd = GetController()->GetCmd();
	int fixed = cmd[1] & 1;
	bool sili = cmd[1] & 2;
	uint32_t transfer_length = GetInt24(cmd, 2);
	if (fixed)
		transfer_length *= GetSectorSizeInBytes();

	uint32_t blocks = transfer_length / GetSectorSizeInBytes();
	if (!sili && ((uint32_t(file.tell()) + transfer_length > GetFileSize())
				  || (!fixed && (transfer_length % GetSectorSizeInBytes() != 0)))) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	GetController()->SetBlocks(blocks);
	GetController()->AllocateBuffer(GetSectorSizeInBytes());
	auto len = file.read(GetController()->GetBuffer().data(), GetSectorSizeInBytes());
	GetController()->SetLength(GetSectorSizeInBytes());

	LogTrace("Length is " + to_string(len));

	// Set next block
	GetController()->SetNext(file.tell() / GetSectorSizeInBytes());

	EnterDataInPhase();
}

void SCSIST::ReadBlockLimits() const
{
	/*
Table 179 - READ BLOCK LIMITS command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (05h)                        |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number    |                  Reserved                  |
|-----+-----------------------------------------------------------------------|
| 2   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 3   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 4   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+

Table 180 - READ BLOCK LIMITS data
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 1   | (MSB)                                                                 |
|-----+---                                                                 ---|
| 2   |                           Maximum block length limit                  |
|-----+---                                                                 ---|
| 3   |                                                                 (LSB) |
|-----+-----------------------------------------------------------------------|
| 4   | (MSB)                                                                 |
|-----+---                        Minimum block length limit               ---|
| 5   |                                                                 (LSB) |
+=============================================================================+
*/
	GetController()->AllocateBuffer(6);
	fill_n(GetController()->GetBuffer().begin(), 6, 0);
	SetInt24(GetController()->GetBuffer(), 1, GetMaxSupportedSectorSize());
	SetInt16(GetController()->GetBuffer(), 4, GetMinSupportedSectorSize());
	GetController()->SetBlocks(1);
	GetController()->SetLength(6);
	EnterDataInPhase();
}

void SCSIST::Rewind()
{
	/*
Table 187 - REWIND command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (01h)                        |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number    |              Reserved             | Immed  |
|-----+-----------------------------------------------------------------------|
| 2   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 3   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 4   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
*/
	file.rewind();
	EnterStatusPhase();
}

void SCSIST::Space()
{
	/*
Table 188 - SPACE command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation  (11h)                            |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number    |     Reserved    |           Code           |
|-----+-----------------------------------------------------------------------|
| 2   | (MSB)                                                                 |
|-----+---                                                                 ---|
| 3   |                           Count                                       |
|-----+---                                                                 ---|
| 4   |                                                                 (LSB) |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
The code field is defined in table 189.

Table 189 - Code field definition
+=============-========================-=============+
|    Code     |  Description           |  Support    |
|-------------+------------------------+-------------|
|    000b     |  Blocks                |  Mandatory  |
|    001b     |  Filemarks             |  Mandatory  |
|    010b     |  Sequential filemarks  |  Optional   |
|    011b     |  End-of-data           |  Optional   |
|    100b     |  Setmarks              |  Optional   |
|    101b     |  Sequential setmarks   |  Optional   |
| 110b - 111b |  Reserved              |             |
+====================================================+
*/
	uint8_t code = GetController()->GetCmd()[1] & 0x07;
	uint32_t count = GetInt24(GetController()->GetCmd(), 2);
	switch (code) {
	case 0: // Blocks
		if (GetFileSize() >= count * GetSectorSizeInBytes()) {
			file.seek(count * GetSectorSizeInBytes(), SEEK_SET);
			break;
		}
		// Fall through
	case 1: // Filemarks
	case 2: // Sequential filemarks
	case 3: // End-of-data
	case 4: // Setmarks
	case 5: // Sequential setmarks
	default:
		// TODO Add proper implementation
		GetController()->Error(sense_key::blank_check, asc::no_additional_sense_information, status::check_condition);
		EnterStatusPhase();
		break;
	}
}

void SCSIST::Write6()
{
	/*
Table 191 - WRITE command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (0Ah)                        |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number    |              Reserved             | Fixed  |
|-----+-----------------------------------------------------------------------|
| 2   | (MSB)                                                                 |
|-----+---                                                                 ---|
| 3   |                           Transfer length                             |
|-----+---                                                                 ---|
| 4   |                                                                 (LSB) |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
*/
	CheckReady();
	if (IsProtected()) {
		throw scsi_exception(sense_key::data_protect, asc::write_protected);
	}
	bool fixed = GetController()->GetCmd()[1] & 1;
	uint32_t length = GetInt24(GetController()->GetCmd(),2);

	if (!fixed) {
		if (length != GetSectorSizeInBytes()) {
			throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
		}
	} else {
		length *= GetSectorSizeInBytes();
	}
	GetController()->SetBlocks(length / GetSectorSizeInBytes());
	GetController()->SetLength(length);

	// Set next block
	GetController()->SetNext(file.tell() / GetSectorSizeInBytes() + 1);

	EnterDataOutPhase();
}

void SCSIST::WriteFilemarks()
{
	/*
Table 191 - WRITE command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (0Ah)                        |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number    |              Reserved             | Fixed  |
|-----+-----------------------------------------------------------------------|
| 2   | (MSB)                                                                 |
|-----+---                                                                 ---|
| 3   |                           Transfer length                             |
|-----+---                                                                 ---|
| 4   |                                                                 (LSB) |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
*/
	// TODO Add proper implementation
	EnterStatusPhase();
}

void SCSIST::LoadUnload()
{
	/*
Table 176 - LOAD UNLOAD command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (1Bh)                        |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number    |              Reserved             | Immed  |
|-----+-----------------------------------------------------------------------|
| 2   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 3   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 4   |                           Reserved         |   EOT  |  Reten |  Load  |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
*/
	bool load = GetController()->GetCmd()[4] & 1;
	bool eot = GetController()->GetCmd()[4] & 4;
	if (load) {
		file.rewind();
	} else if (eot) {
		file.seek(0, SEEK_END);
	}
	if (load & eot){
		GetController()->Error(sense_key::illegal_request, asc::no_additional_sense_information, status::check_condition);
	}
	EnterStatusPhase();
}

void SCSIST::ReadPosition()
{
/*
Table 181 - READ POSITION command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (34h)                        |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number    |             Reserved              |   BT   |
|-----+-----------------------------------------------------------------------|
| 2   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 3   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 4   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 5   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 6   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 7   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 8   |                           Reserved                                    |
|-----+-----------------------------------------------------------------------|
| 9   |                           Control                                     |
+=============================================================================+
*/
	GetController()->AllocateBuffer(20);
	fill_n(GetController()->GetBuffer().begin(), 6, 0);
	size_t lba= file.tell() / GetSectorSizeInBytes();
	auto &buf = GetController()->GetBuffer();
	if (!lba)
		buf[0] |= 0x80;
	else if (lba >= GetBlockCount())
		buf[0] |= 0x40;

	SetInt32(GetController()->GetBuffer(), 4, lba);
	SetInt32(GetController()->GetBuffer(), 8, lba);

	EnterDataInPhase();
}

void SCSIST::Verify()
{
	/*
Table 190 - VERIFY command
+=====-========-========-========-========-========-========-========-========+
|  Bit|   7    |   6    |   5    |   4    |   3    |   2    |   1    |   0    |
|Byte |        |        |        |        |        |        |        |        |
|=====+=======================================================================|
| 0   |                           Operation code (13h)                        |
|-----+-----------------------------------------------------------------------|
| 1   |   Logical unit number    |    Reserved     | Immed  | BytCmp | Fixed  |
|-----+-----------------------------------------------------------------------|
| 2   | (MSB)                                                                 |
|-----+---                                                                 ---|
| 3   |                           Verification length                         |
|-----+---                                                                 ---|
| 4   |                                                                 (LSB) |
|-----+-----------------------------------------------------------------------|
| 5   |                           Control                                     |
+=============================================================================+
*/
	// TODO Add proper implementation
	EnterStatusPhase();
}

void SCSIST::SetUpModePages(map<int, vector<byte>>& pages, int page, bool changeable) const
{
	// Page code 0 is returning the Header List followed by the Block Descriptor List - a total of 12 bytes.
	// When selecting Page Code OOh the DBD bit is ignored.
	if (page == 0x00) {
		AddBlockDescriptorPage(pages, changeable);
	}


	// Page code 1 (read-write error recovery)
	if (page == 0x01) {
		AddErrorPage(pages, changeable);
	}

	// Page code 2 (disconnect/reconnect)
	if (page == 0x02) {
		AddReconnectPage(pages, changeable);
	}

	// Page code 0x10 (Device Configuration)
	if (page == 0x10) {
		AddDevicePage(pages, changeable);
	}

	// Page code 0x11 (Medium Partition)
	if (page == 0x11) {
		AddMediumPartitionPage(pages, changeable);
	}

	// Page code 0x20 (Miscellaneous)
	if (page == 0x20) {
		AddMiscellaneousPage(pages, changeable);
	}

	// Page (vendor special)
	AddVendorPage(pages, page, changeable);
}

void SCSIST::Write(span<const uint8_t> buff, uint64_t block)
{
	assert(block < GetBlockCount());

	CheckReady();

	file.seek(block * GetSectorSizeInBytes(), SEEK_SET);
	file.write(buff.data(), GetSectorSizeInBytes());
}

int SCSIST::Read(span<uint8_t>buff, uint64_t block)
{
	assert(block < GetBlockCount());
	file.seek(block * GetSectorSizeInBytes(), SEEK_SET);
	file.read(buff.data(), GetSectorSizeInBytes());
	return GetSectorSizeInBytes();
}


void SCSIST::Open()
{
	assert(!IsReady());
	// Sector size (default 512 bytes) and number of blocks
	SetSectorSizeInBytes(GetConfiguredSectorSize() ? GetConfiguredSectorSize() : 512);
	off_t size = GetFileSize();
	if (size % GetSectorSizeInBytes() != 0) {
		size = (size / GetSectorSizeInBytes() + 1) * GetSectorSizeInBytes();
	}
	SetBlockCount(static_cast<uint32_t>(size >> GetSectorSizeShiftCount()));
	file.open(GetFilename());
	StorageDevice::ValidateFile();
}

void SCSIST::AddBlockDescriptorPage(std::map<int, std::vector<byte> > &pages, bool) const
{
	// Page Code OOh is returning the Header List followed by the Block Descriptor List - a total of 12 bytes.
	// When selecting Page Code OOh the DBD bit is ignored.
	vector<byte> buf(12);
	buf[2] = (byte)0; // Buffered Mode | Tape Speed
	buf[3] = (byte)8; // Block descriptor length
	buf[4] = (byte)0; // Density Code
	SetInt24(buf, 5, GetBlockCount()); // Number of Blocks
	buf[8] = (byte)0; // Reserved
	SetInt24(buf, 9, GetSectorSizeInBytes()); // Block Length
	pages[0] = buf;
}

void SCSIST::AddErrorPage(map<int, vector<byte> > &pages, bool) const
{
	vector<byte> buf(12);

	buf[0] = (byte)0x01; // Page code
	buf[1] = (byte)0x0a; // Page length
	buf[2] = (byte)0x00; // RCR, EXB, TB, PER, DTE
	buf[3] = (byte)0x01; // Read retry count
	buf[8] = (byte)0x01; // Write retry count

	pages[1] = buf;
}

void SCSIST::AddReconnectPage(map<int, vector<byte> > &pages, bool) const
{
	vector<byte> buf(16);

	buf[0] = (byte)0x02; // Page code
	buf[1] = (byte)0x0e; // Page length
	buf[2] = (byte)0x00; // Read Buffer Full Ratio
	buf[3] = (byte)0x00; // Write Buffer Empty Ratio

	pages[2] = buf;
}

void SCSIST::AddDevicePage(map<int, vector<byte> > &pages, bool) const
{
	vector<byte> buf(16);

	buf[0] = (byte)0x10; // Page code
	buf[1] = (byte)0x0e; // Page length
	buf[2] = (byte)0x00; // CAP, CAF, Active Format
	buf[3] = (byte)0x00; // Active Partition
	buf[4] = (byte)0x00; // Write Buffer Full Ratio
	buf[5] = (byte)0x00; // Read Buffer Empty Ratio
	SetInt16(buf, 6, 0x0000); // Write Buffer Empty Ratio
	buf[8] = (byte)0b11100000; // DBR, BIS, RSMK, AVC, SOCF, RBO, REW
	buf[9] = (byte)0x00; // Gap Size
	buf[10] = (byte)0b00111000; // EOD Defined, EEG, SEW, RESERVED
	buf[11] = buf[12] = buf[13] = (byte)0x00; // Buffer Size
	buf[14] = (byte)0x00; // Select Data Compression Algorithm
	pages[0x10] = buf;
}

void SCSIST::AddMediumPartitionPage(map<int, vector<byte> > &pages, bool) const
{
	vector<byte> buf(8);

	buf[0] = (byte)0x11; // Page code
	buf[1] = (byte)0x06; // Page length
	buf[2] = (byte)0x01; // Maximum Additional Partitions
	buf[3] = (byte)0x00; // Additional Partition Length

	pages[0x11] = buf;
}

void SCSIST::AddMiscellaneousPage(map<int, vector<byte> > &pages, bool) const
{
	vector<byte> buf(12);

	buf[0] = (byte)0x12; // Page code
	buf[1] = (byte)0x0a; // Page length
	SetInt16(buf, 2, 0x0001); // Maximum Additional Partitions
	buf[4] = (byte)0x18; // ASI, Target Sense Length
	buf[5] = (byte)0x08; // Copy Threshold
	buf[6] = (byte)0x00; // Load Function
	buf[7] = (byte)0x00; // Power-Up Auto Load/Retension Delay
	buf[8] = (byte)0b10000000; // DTM1, DTM2, SPEW, EOWR EADS, BSY, RD, FAST
	buf[9] = (byte)0x00; // LED Function
	buf[10] = (byte)0x00; // PSEW Position
	pages[0x12] = buf;
}
