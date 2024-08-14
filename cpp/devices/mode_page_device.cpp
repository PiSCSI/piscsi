//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
// A basic device with mode page support, to be used for subclassing
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "scsi_command_util.h"
#include "mode_page_device.h"
#include <cstddef>
#include <sstream>
#include <iomanip>

using namespace std;
using namespace scsi_defs;
using namespace scsi_command_util;

bool ModePageDevice::Init(const param_map& params)
{
	PrimaryDevice::Init(params);

	AddCommand(scsi_command::eCmdModeSense6, [this] { ModeSense6(); });
	AddCommand(scsi_command::eCmdModeSense10, [this] { ModeSense10(); });
	AddCommand(scsi_command::eCmdModeSelect6, [this] { ModeSelect6(); });
	AddCommand(scsi_command::eCmdModeSelect10, [this] { ModeSelect10(); });

	return true;
}

int ModePageDevice::AddModePages(cdb_t cdb, vector<uint8_t>& buf, int offset, int length, int max_size) const
{
	const int max_length = length - offset;
	if (max_length < 0) {
		return length;
	}

	const bool changeable = (cdb[2] & 0xc0) == 0x40;

	// Get page code (0x3f means all pages)
	const int page = cdb[2] & 0x3f;

	stringstream s;
	s << "Requesting mode page $" << setfill('0') << setw(2) << hex << page;
	LogTrace(s.str());

	// Mode page data mapped to the respective page numbers, C++ maps are ordered by key
	map<int, vector<byte>> pages;
	SetUpModePages(pages, page, changeable);

	if (pages.empty()) {
		s << "Unsupported mode page $" << page;
		LogTrace(s.str());
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	// Holds all mode page data
	vector<byte> result;

	vector<byte> page0;
	for (const auto& [index, data] : pages) {
		// The specification mandates that page 0 must be returned after all others
		if (index) {
			const size_t off = result.size();

			// Page data
			result.insert(result.end(), data.begin(), data.end());
			// Page code, PS bit may already have been set
			result[off] |= (byte)index;
			// Page payload size
			result[off + 1] = (byte)(data.size() - 2);
		}
		else {
			page0 = data;
		}
	}

	// Page 0 must be last
	if (!page0.empty()) {
		const size_t off = result.size();

		// Page data
		result.insert(result.end(), page0.begin(), page0.end());
		// Page payload size
		result[off + 1] = (byte)(page0.size() - 2);
	}

	if (static_cast<int>(result.size()) > max_size) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	const auto size = static_cast<int>(min(static_cast<size_t>(max_length), result.size()));
	memcpy(&buf.data()[offset], result.data(), size);

	// Do not return more than the requested number of bytes
	return size + offset < length ? size + offset : length;
}

void ModePageDevice::ModeSense6() const
{
	GetController()->SetLength(ModeSense6(GetController()->GetCmd(), GetController()->GetBuffer()));

	EnterDataInPhase();
}

void ModePageDevice::ModeSense10() const
{
	GetController()->SetLength(ModeSense10(GetController()->GetCmd(), GetController()->GetBuffer()));

	EnterDataInPhase();
}

void ModePageDevice::ModeSelect(scsi_command, cdb_t, span<const uint8_t>, int)
{
	// There is no default implementation of MODE SELECT
	throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
}

void ModePageDevice::ModeSelect6() const
{
	SaveParametersCheck(GetController()->GetCmdByte(4));
}

void ModePageDevice::ModeSelect10() const
{
	const auto length = min(GetController()->GetBuffer().size(), static_cast<size_t>(GetInt16(GetController()->GetCmd(), 7)));

	SaveParametersCheck(static_cast<uint32_t>(length));
}

void ModePageDevice::SaveParametersCheck(int length) const
{
	if (!SupportsSaveParameters() && (GetController()->GetCmdByte(1) & 0x01)) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	GetController()->SetLength(length);

	EnterDataOutPhase();
}
