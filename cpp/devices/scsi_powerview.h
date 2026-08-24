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

#pragma once

#include "primary_device.h"
#include <cstddef>
#include <span>
#include <vector>

class SCSIPowerView : public PrimaryDevice
{
public:
	static constexpr size_t MAX_FRAMEBUFFER_BYTES = 800 * 600;
	static constexpr size_t UNKNOWN_CC_LENGTH = 0x8bb;

	explicit SCSIPowerView(int lun);
	~SCSIPowerView() override = default;

	bool Init(const param_map&) override;
	void Reset() override;

	bool WriteByteSequence(span<const uint8_t>) override;

private:
	enum class transfer_t {
		none,
		configuration,
		framebuffer,
		palette,
		unknown_cc
	};

	vector<uint8_t> InquiryInternal() const override;

	void ReadConfiguration();
	void WriteConfiguration();
	void WriteFrameBuffer();
	void WriteColorPalette();
	void WriteUnknownCC();
	void StartDataOut(transfer_t, size_t);

	transfer_t pending_transfer = transfer_t::none;
	size_t pending_transfer_length = 0;

	// Retained for diagnostic use and for the video model added in the next phase.
	vector<uint8_t> configuration_data;
	vector<uint8_t> palette_data;
	vector<uint8_t> unknown_cc_data;
};
