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
#include <array>
#include <chrono>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

class SCSIPowerView : public PrimaryDevice
{
public:
	using color_t = array<uint8_t, 3>;

	static constexpr size_t MAX_WIDTH = 800;
	static constexpr size_t MAX_HEIGHT = 600;
	static constexpr size_t MAX_FRAMEBUFFER_BYTES = MAX_WIDTH * MAX_HEIGHT;
	static constexpr size_t UNKNOWN_CC_LENGTH = 0x8bb;

	explicit SCSIPowerView(int lun);
	~SCSIPowerView() override = default;

	bool Init(const param_map&) override;
	void Reset() override;
	param_map GetDefaultParams() const override;

	bool WriteByteSequence(span<const uint8_t>) override;

	uint16_t GetScreenWidth() const { return screen_width; }
	uint16_t GetScreenHeight() const { return screen_height; }
	uint8_t GetPixel(size_t x, size_t y) const { return framebuffer[y * MAX_WIDTH + x]; }
	const array<color_t, 256>& GetPalette() const { return palette; }

private:
	enum class pixel_format_t {
		one_bit,
		four_bit,
		eight_bit
	};

	enum class transfer_t {
		none,
		configuration,
		framebuffer,
		palette,
		unknown_cc
	};

	struct framebuffer_update_t {
		size_t width_bytes;
		 size_t height;
		 size_t row;
		 size_t column;
		 size_t width_pixels;
		 bool full_refresh;
	};

	vector<uint8_t> InquiryInternal() const override;

	void ReadConfiguration();
	void WriteConfiguration();
	void WriteFrameBuffer();
	void WriteColorPalette();
	void WriteUnknownCC();
	void StartDataOut(transfer_t, size_t);
	void ApplyFrameBufferUpdate(span<const uint8_t>);
	void ApplyPalette(span<const uint8_t>);
	optional<framebuffer_update_t> GetFrameBufferUpdate() const;
	bool SetScreenDimensions(size_t width, size_t height);
	void ClearVideoState();
	void WriteSnapshot(bool);

	transfer_t pending_transfer = transfer_t::none;
	size_t pending_transfer_length = 0;
	optional<framebuffer_update_t> pending_framebuffer_update;

	uint16_t screen_width = 640;
	uint16_t screen_height = 400;
	pixel_format_t pixel_format = pixel_format_t::one_bit;
	array<uint8_t, MAX_FRAMEBUFFER_BYTES> framebuffer {};
	array<color_t, 256> palette {};

	string snapshot_path;
	chrono::milliseconds snapshot_interval { 250 };
	chrono::steady_clock::time_point last_snapshot {};
	bool snapshot_full_refresh_only = false;

	// Retained for diagnostic use.
	vector<uint8_t> configuration_data;
	vector<uint8_t> palette_data;
	vector<uint8_t> unknown_cc_data;
};
