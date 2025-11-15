//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "scsi_command_util.h"
#include <spdlog/spdlog.h>
#include <cstring>
#include <sstream>
#include <iomanip>

using namespace scsi_defs;

string scsi_command_util::ModeSelect(scsi_command cmd, cdb_t cdb, span<const uint8_t> buf, int length, int sector_size)
{
	assert(cmd == scsi_command::eCmdModeSelect6 || cmd == scsi_command::eCmdModeSelect10);
	assert(length >= 0);

	string result;

	// PF
	if (!(cdb[1] & 0x10)) {
		// Vendor-specific parameters (SCSI-1) are not supported.
		// Do not report an error in order to support Apple's HD SC Setup.
		return result;
	}

	// The parameter list must include a complete MODE SELECT header and be present in the transfer buffer.
	// SCSI-2 8.2.8 requires PARAMETER LIST LENGTH ERROR for any truncated header, descriptor, or page.
	const int header_length = cmd == scsi_command::eCmdModeSelect10 ? 8 : 4;
	if (length < header_length || length > static_cast<int>(buf.size())) {
		throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
	}

	// Skip block descriptors
	const int block_descriptor_length = cmd == scsi_command::eCmdModeSelect10 ? GetInt16(buf, 6) : buf[3];
	if (block_descriptor_length > length - header_length) {
		throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
	}

	int offset = header_length + block_descriptor_length;
	length -= offset;

	bool has_valid_page_code = false;

	// Parse the pages
	while (length > 0) {
		// A mode page header must not be truncated.
		if (length < 2) {
			throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
		}

		const int size = buf[offset + 1] + 2;
		if (size > length) {
			throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
		}

		// Format device page
		if (const int page = buf[offset]; page == 0x03) {
			// The sector size field is at offset 12 in the page and occupies two bytes.
			if (size < 14) {
				throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
			}

			// With this page the sector size for a subsequent FORMAT can be selected, but only very few
			// drives support this, e.g FUJITSU M2624S
			// We are fine as long as the current sector size remains unchanged
			if (GetInt16(buf, offset + 12) != sector_size) {
				// With piscsi it is not possible to permanently (by formatting) change the sector size,
				// because the size is an externally configurable setting only
				spdlog::warn("In order to change the sector size use the -b option when launching piscsi");
				throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
			}

			has_valid_page_code = true;
		}
		else {
			stringstream s;
			s << "Unknown MODE SELECT page code: $" << setfill('0') << setw(2) << hex << page;
			result = s.str();
		}

		// Advance to the next page
		length -= size;
		offset += size;
	}

	if (!has_valid_page_code) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
	}

	return result;
}

void scsi_command_util::EnrichFormatPage(map<int, vector<byte>>& pages, bool changeable, int sector_size)
{
	if (changeable) {
		// The sector size is simulated to be changeable, see the MODE SELECT implementation for details
		SetInt16(pages[3], 12, sector_size);
	}
}

void scsi_command_util::AddAppleVendorModePage(map<int, vector<byte>>& pages, bool changeable)
{
	// Page code 48 (30h) - Apple Vendor Mode Page
	// Needed for SCCD for stock Apple driver support
	// Needed for SCHD for stock Apple HD SC Setup
	pages[48] = vector<byte>(30);

	// No changeable area
	if (!changeable) {
		constexpr const char APPLE_DATA[] = "APPLE COMPUTER, INC   ";
		memcpy(&pages[48].data()[2], APPLE_DATA, sizeof(APPLE_DATA));
	}
}



uint32_t scsi_command_util::GetInt32(span <const int> buf, int offset)
{
	assert(buf.size() > static_cast<size_t>(offset) + 3);

	return (static_cast<uint32_t>(buf[offset]) << 24) | (static_cast<uint32_t>(buf[offset + 1]) << 16) |
			(static_cast<uint32_t>(buf[offset + 2]) << 8) | static_cast<uint32_t>(buf[offset + 3]);
}

uint64_t scsi_command_util::GetInt64(span<const int> buf, int offset)
{
	assert(buf.size() > static_cast<size_t>(offset) + 7);

	return (static_cast<uint64_t>(buf[offset]) << 56) | (static_cast<uint64_t>(buf[offset + 1]) << 48) |
			(static_cast<uint64_t>(buf[offset + 2]) << 40) | (static_cast<uint64_t>(buf[offset + 3]) << 32) |
			(static_cast<uint64_t>(buf[offset + 4]) << 24) | (static_cast<uint64_t>(buf[offset + 5]) << 16) |
			(static_cast<uint64_t>(buf[offset + 6]) << 8) | static_cast<uint64_t>(buf[offset + 7]);
}

void scsi_command_util::SetInt64(vector<uint8_t>& buf, int offset, uint64_t value)
{
	assert(buf.size() > static_cast<size_t>(offset) + 7);

	buf[offset] = static_cast<uint8_t>(value >> 56);
	buf[offset + 1] = static_cast<uint8_t>(value >> 48);
	buf[offset + 2] = static_cast<uint8_t>(value >> 40);
	buf[offset + 3] = static_cast<uint8_t>(value >> 32);
	buf[offset + 4] = static_cast<uint8_t>(value >> 24);
	buf[offset + 5] = static_cast<uint8_t>(value >> 16);
	buf[offset + 6] = static_cast<uint8_t>(value >> 8);
	buf[offset + 7] = static_cast<uint8_t>(value);
}
