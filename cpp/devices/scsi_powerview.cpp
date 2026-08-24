//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI for Raspberry Pi
//
// Copyright (C) 2026 Daniel Markstedt <daniel@mindani.net>
// Copyright (C) 2020-2023 akuker
// Copyright (C) 2020 joshua stein <jcs@jcs.org>
//
// Licensed under the BSD 3-Clause License.
// See LICENSE file in the project root folder.
//
// Emulation of the Radius PowerView SCSI display adapter
//
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "scsi_powerview.h"
#include <algorithm>
#include <array>

using namespace scsi_defs;

namespace
{
constexpr array<uint8_t, 0x4b> inquiry_response = {
	0x03, 0x00, 0x01, 0x01, 0x46, 0x00, 0x00, 0x00,
	0x52, 0x41, 0x44, 0x49, 0x55, 0x53, 0x20, 0x20,
	0x50, 0x6f, 0x77, 0x65, 0x72, 0x56, 0x69, 0x65,
	0x77, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x56, 0x31, 0x2e, 0x30, 0x00, 0x00, 0x00, 0x00,
	0x10, 0x00, 0x20, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
	0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
	0x05, 0x00, 0x00, 0x00, 0x00, 0x06, 0x43, 0xf9,
	0x00, 0x00, 0xff
};
}

SCSIPowerView::SCSIPowerView(int lun) : PrimaryDevice(SCPV, lun)
{
	SetReady(true);
}

bool SCSIPowerView::Init(const param_map& params)
{
	if (!PrimaryDevice::Init(params)) {
		return false;
	}

	AddCommand(scsi_command::eCmdPowerViewReadConfig, [this] { ReadConfiguration(); });
	AddCommand(scsi_command::eCmdPowerViewWriteConfig, [this] { WriteConfiguration(); });
	AddCommand(scsi_command::eCmdPowerViewWriteFrameBuffer, [this] { WriteFrameBuffer(); });
	AddCommand(scsi_command::eCmdPowerViewWriteColorPalette, [this] { WriteColorPalette(); });
	AddCommand(scsi_command::eCmdPowerViewUnknownCC, [this] { WriteUnknownCC(); });

	return true;
}

void SCSIPowerView::Reset()
{
	pending_transfer = transfer_t::none;
	pending_transfer_length = 0;

	PrimaryDevice::Reset();
}

vector<uint8_t> SCSIPowerView::InquiryInternal() const
{
	return { inquiry_response.begin(), inquiry_response.end() };
}

void SCSIPowerView::ReadConfiguration()
{
	const size_t length = static_cast<uint8_t>(GetController()->GetCmdByte(6));
	GetController()->AllocateBuffer(length);

	auto& buffer = GetController()->GetBuffer();
	fill_n(buffer.begin(), length, 0);

	if (GetController()->GetCmdByte(3) == 0x31) {
		switch (GetController()->GetCmdByte(4)) {
		case 0x00: {
			constexpr array<uint8_t, 3> response = { 0x01, 0x09, 0x08 };
			copy_n(response.begin(), min(response.size(), length), buffer.begin());
			break;
		}

		case 0x82:
			if (length) {
				buffer[0] = 0x01;
			}
			break;

		case 0x83:
			// The recorded response is all zeroes.
			break;

		default:
			LogWarn("Unhandled PowerView C8 configuration selector");
			break;
		}
	}

	GetController()->SetBlocks(1);
	GetController()->SetLength(static_cast<uint32_t>(length));
	EnterDataInPhase();
}

void SCSIPowerView::WriteConfiguration()
{
	const size_t length = static_cast<uint8_t>(GetController()->GetCmdByte(6));
	if (!length) {
		EnterStatusPhase();
		return;
	}

	StartDataOut(transfer_t::configuration, length);
}

void SCSIPowerView::WriteFrameBuffer()
{
	const size_t width = (static_cast<size_t>(GetController()->GetCmdByte(4)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(5));
	const size_t height = (static_cast<size_t>(GetController()->GetCmdByte(6)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(7));

	if (!width || !height || width > MAX_FRAMEBUFFER_BYTES / height) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	const size_t length = width * height;
	if (length > MAX_FRAMEBUFFER_BYTES) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	StartDataOut(transfer_t::framebuffer, length);
}

void SCSIPowerView::WriteColorPalette()
{
	const size_t entries = (static_cast<size_t>(GetController()->GetCmdByte(3)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(4));
	if (!entries || entries > 256) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	// The black-and-white mode advertises one entry, but transfers black and white.
	const size_t length = entries == 1 ? 8 : entries * 4;
	StartDataOut(transfer_t::palette, length);
}

void SCSIPowerView::WriteUnknownCC()
{
	StartDataOut(transfer_t::unknown_cc, UNKNOWN_CC_LENGTH);
}

void SCSIPowerView::StartDataOut(transfer_t transfer, size_t length)
{
	GetController()->AllocateBuffer(length);
	GetController()->SetBlocks(1);
	GetController()->SetLength(static_cast<uint32_t>(length));
	GetController()->SetByteTransfer(true);
	pending_transfer = transfer;
	pending_transfer_length = length;
	EnterDataOutPhase();
}

bool SCSIPowerView::WriteByteSequence(span<const uint8_t> data)
{
	if (pending_transfer == transfer_t::none || data.size() != pending_transfer_length) {
		LogWarn("Unexpected PowerView data-out payload");
		return false;
	}

	switch (pending_transfer) {
	case transfer_t::configuration:
		configuration_data.assign(data.begin(), data.end());
		break;

	case transfer_t::palette:
		palette_data.assign(data.begin(), data.end());
		break;

	case transfer_t::unknown_cc:
		unknown_cc_data.assign(data.begin(), data.end());
		break;

	case transfer_t::framebuffer:
		// Framebuffer decoding is implemented by the in-memory video model in step 3.
		break;

	case transfer_t::none:
		return false;
	}

	pending_transfer = transfer_t::none;
	pending_transfer_length = 0;
	return true;
}
