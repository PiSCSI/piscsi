//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "rascsi_exceptions.h"
#include "scsi_command_util.h"

using namespace scsi_defs;

void scsi_command_util::ModeSelect(const vector<int>& cdb, const vector<BYTE>& buf, int length, int sector_size)
{
	assert(length >= 0);

	// PF
	if (!(cdb[1] & 0x10)) {
		// Vendor-specific parameters (SCSI-1) are not supported
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
	}

	bool has_valid_page_code = false;

	// Mode Parameter header
	int offset = 0;
	if (length >= 12) {
		// Check the block length
		if (buf[9] != (BYTE)(sector_size >> 16) || buf[10] != (BYTE)(sector_size >> 8) ||
				buf[11] != (BYTE)sector_size) {
			// See below for details
			LOGWARN("In order to change the sector size use the -b option when launching rascsi")
			throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
		}

		offset += 12;
		length -= 12;
	}

	// Parsing the page
	// TODO The length handling is wrong in case of length < size
	while (length > 0) {
		// Format device page
		if (int page = buf[offset]; page == 0x03) {
			// With this page the sector size for a subsequent FORMAT can be selected, but only very few
			// drives support this, e.g FUJITSU M2624S
			// We are fine as long as the current sector size remains unchanged
			if (buf[offset + 0xc] != (BYTE)(sector_size >> 8) || buf[offset + 0xd] != (BYTE)sector_size) {
				// With rascsi it is not possible to permanently (by formatting) change the sector size,
				// because the size is an externally configurable setting only
				LOGWARN("In order to change the sector size use the -b option when launching rascsi")
				throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
			}

			has_valid_page_code = true;
		}
		else {
			LOGWARN("Unknown MODE SELECT page code: $%02X", page)
		}

		// Advance to the next page
		int size = buf[offset + 1] + 2;
		length -= size;
		offset += size;
	}

	if (!has_valid_page_code) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
	}
}

void scsi_command_util::EnrichFormatPage(map<int, vector<byte>>& pages, bool changeable, int sector_size)
{
	if (changeable) {
		// The sector size is simulated to be changeable, see the MODE SELECT implementation for details
		vector<byte>& format_page = pages[3];
		format_page[12] = (byte)(sector_size >> 8);
		format_page[13] = (byte)sector_size;
	}
}

void scsi_command_util::AddAppleVendorModePage(map<int, vector<byte>>& pages, bool changeable)
{
	// Page code 48 (30h) - Apple Vendor Mode Page
	// Needed for SCCD for stock Apple driver support
	// Needed for SCHD for stock Apple HD SC Setup
	vector<byte> buf(30);

	// No changeable area
	if (!changeable) {
		const char APPLE_DATA[] = "APPLE COMPUTER, INC   ";
		memcpy(&buf[2], APPLE_DATA, sizeof(APPLE_DATA));
	}

	pages[48] = buf;
}

int scsi_command_util::GetInt16(const vector<int>& buf, int offset)
{
	return (buf[offset] << 8) | buf[offset + 1];
}

int scsi_command_util::GetInt24(const vector<int>& buf, int offset)
{
	return (buf[offset] << 16) | (buf[offset + 1] << 8) | buf[offset + 2];
}

uint32_t scsi_command_util::GetInt32(const vector<int>& buf, int offset)
{
	return ((uint32_t)buf[offset] << 24) | ((uint32_t)buf[offset + 1] << 16) |
			((uint32_t)buf[offset + 2] << 8) | (uint32_t)buf[offset + 3];
}

uint64_t scsi_command_util::GetInt64(const vector<int>& buf, int offset)
{
	return ((uint64_t)buf[offset] << 56) | ((uint64_t)buf[offset + 1] << 48) |
			((uint64_t)buf[offset + 2] << 40) | ((uint64_t)buf[offset + 3] << 32) |
			((uint64_t)buf[offset + 4] << 24) | ((uint64_t)buf[offset + 5] << 16) |
			((uint64_t)buf[offset + 6] << 8) | (uint64_t)buf[offset + 7];
}

void scsi_command_util::SetInt16(vector<byte>& buf, int offset, int value)
{
	buf[offset] = (byte)(value >> 8);
	buf[offset + 1] = (byte)value;
}

void scsi_command_util::SetInt32(vector<byte>& buf, int offset, uint32_t value)
{
	buf[offset] = (byte)(value >> 24);
	buf[offset + 1] = (byte)(value >> 16);
	buf[offset + 2] = (byte)(value >> 8);
	buf[offset + 3] = (byte)value;
}

void scsi_command_util::SetInt16(vector<BYTE>& buf, int offset, int value)
{
	buf[offset] = (BYTE)(value >> 8);
	buf[offset + 1] = (BYTE)value;
}

void scsi_command_util::SetInt32(vector<BYTE>& buf, int offset, uint32_t value)
{
	buf[offset] = (BYTE)(value >> 24);
	buf[offset + 1] = (BYTE)(value >> 16);
	buf[offset + 2] = (BYTE)(value >> 8);
	buf[offset + 3] = (BYTE)value;
}

void scsi_command_util::SetInt64(vector<BYTE>& buf, int offset, uint64_t value)
{
	buf[offset] = (BYTE)(value >> 56);
	buf[offset + 1] = (BYTE)(value >> 48);
	buf[offset + 2] = (BYTE)(value >> 40);
	buf[offset + 3] = (BYTE)(value >> 32);
	buf[offset + 4] = (BYTE)(value >> 24);
	buf[offset + 5] = (BYTE)(value >> 16);
	buf[offset + 6] = (BYTE)(value >> 8);
	buf[offset + 7] = (BYTE)value;
}
