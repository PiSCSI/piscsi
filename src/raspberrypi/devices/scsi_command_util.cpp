//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "exceptions.h"
#include "scsi_command_util.h"

using namespace scsi_defs;

void scsi_command_util::ModeSelect(const DWORD *cdb, const BYTE *buf, int length, int sector_size)
{
	assert(length >= 0);

	// PF
	if (cdb[1] & 0x10) {
		bool has_valid_page_code = false;

		// Mode Parameter header
		if (length >= 12) {
			if (buf[9] != (BYTE)(sector_size >> 16) || buf[10] != (BYTE)(sector_size >> 8) ||
					buf[11] != (BYTE)sector_size) {
				// See below for details
				throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
			}
			has_valid_page_code = true;

			buf += 12;
			length -= 12;
		}

		// Parsing the page
		while (length > 0) {
			int page = buf[0];

			switch (page) {
				// Format device page
				case 0x03: {
					// With this page the sector size for a subsequent FORMAT can be selected, but only very few
					// drives support this, e.g FUJITSU M2624S
					// We are fine as long as the current sector size remains unchanged
					if (buf[0xc] != (BYTE)(sector_size >> 8) || buf[0xd] != (BYTE)sector_size) {
						// With rascsi it is not possible to permanently (by formatting) change the sector size,
						// because the size is an externally configurable setting only
						throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_PARAMETER_LIST);
					}

					has_valid_page_code = true;
					}
					break;

				// vendor unique format
				case 0x20:
					// TODO Is it correct to ignore this? The initiator would rely on something to actually have been changed.
					// just ignore, for now
                    LOGWARN("Ignored MODE SELECT page code: $%02X", page);
					has_valid_page_code = true;
					break;

				default:
					LOGWARN("Unknown MODE SELECT page code: $%02X", page);
					break;
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
}
