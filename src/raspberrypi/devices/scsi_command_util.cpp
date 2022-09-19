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

void scsi_command_util::ModeSelect(const vector<int>& cdb, const BYTE *buf, int length, int sector_size)
{
	assert(length >= 0);

	// PF
	if (cdb[1] & 0x10) {
		bool has_valid_page_code = false;

		// Mode Parameter header
		if (length >= 12) {
			// Check the block length
			if (buf[9] != (BYTE)(sector_size >> 16) || buf[10] != (BYTE)(sector_size >> 8) ||
					buf[11] != (BYTE)sector_size) {
				// See below for details
				LOGWARN("In order to change the sector size use the -b option when launching rascsi")
				throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
			}

			buf += 12;
			length -= 12;
		}

		// Parsing the page
		// TODO The length handling is wrong in case of length < size
		while (length > 0) {
			// Format device page
			if (int page = buf[0]; page == 0x03) {
				// With this page the sector size for a subsequent FORMAT can be selected, but only very few
				// drives support this, e.g FUJITSU M2624S
				// We are fine as long as the current sector size remains unchanged
				if (buf[0xc] != (BYTE)(sector_size >> 8) || buf[0xd] != (BYTE)sector_size) {
					// With rascsi it is not possible to permanently (by formatting) change the sector size,
					// because the size is an externally configurable setting only
					LOGWARN("In order to change the sector size use the -b option when launching rascsi")
					throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
				}

				has_valid_page_code = true;
			}
			else {
				LOGWARN("Unknown MODE SELECT page code: $%02X", page)
			}

			// Advance to the next page
			int size = buf[1] + 2;
			length -= size;
			buf += size;
		}

		if (!has_valid_page_code) {
			throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
		}
	}
	else {
		// Vendor-specific parameters (SCSI-1) are not supported
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
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

void scsi_command_util::AddAppleVendorModePage(map<int, vector<byte>>& pages, int, bool changeable)
{
	// Page code 48 (30h) - Apple Vendor Mode Page
	// Needed for SCCD for stock Apple driver support
	// Needed for SCHD for stock Apple HD SC Setup
	vector<byte> buf(30);

	// No changeable area
	if (!changeable) {
		BYTE apple_data[] = "APPLE COMPUTER, INC   ";
		memcpy(&buf[2], apple_data, sizeof(apple_data));
	}

	pages[0x30] = buf;
}
