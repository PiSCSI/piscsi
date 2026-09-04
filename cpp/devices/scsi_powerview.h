//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI for Raspberry Pi
//
// Copyright (C) 2026 Daniel Markstedt <daniel@mindani.net>
// Copyright (C) 2026 Eric Helgeson
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
#include <condition_variable>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <thread>
#include <vector>

class SCSIPowerView : public PrimaryDevice
{
public:
	using color_t = array<uint8_t, 3>;

	static constexpr size_t MAX_WIDTH = 1152;
	static constexpr size_t MAX_HEIGHT = 882;
	static constexpr size_t MAX_FRAMEBUFFER_BYTES = MAX_WIDTH * MAX_HEIGHT;
	explicit SCSIPowerView(int lun);
	~SCSIPowerView() override;

	bool Init(const param_map&) override;
	void Reset() override;
	param_map GetDefaultParams() const override;

	bool WriteByteSequence(span<const uint8_t>) override;

	uint16_t GetScreenWidth() const { return screen_width; }
	uint16_t GetScreenHeight() const { return screen_height; }
	uint8_t GetPixel(size_t x, size_t y) const { return framebuffer[y * MAX_WIDTH + x]; }
	const array<color_t, 256>& GetPalette() const { return palette; }

	struct mode_descriptor_t {
		uint8_t code;
		uint16_t width;
		uint16_t height;
	};

private:
	enum class pixel_format_t {
		one_bit,
		four_bit,
		eight_bit
	};

	enum class transfer_t {
		none,
		configuration,
		v21_write,
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

	struct snapshot_t {
		uint16_t width;
		uint16_t height;
		vector<uint8_t> pixels;
		array<color_t, 256> palette;
	};

	vector<uint8_t> InquiryInternal() const override;

	void Read6() const;
	void ReadConfiguration();
	void WriteConfiguration();
	void ReadV21MonitorMode();
	void WriteV21Handshake();
	void V21ModeSet();
	void QuadraSetup();
	void WriteFrameBuffer();
	void WriteColorPalette();
	void WriteUnknownCC();
	void StartDataOut(transfer_t, size_t);
	void ApplyFrameBufferUpdate(span<const uint8_t>);
	void ApplyPalette(span<const uint8_t>);
	optional<framebuffer_update_t> GetFrameBufferUpdate() const;
	const mode_descriptor_t& GetActiveMode() const;
	static optional<mode_descriptor_t> GetMode(uint8_t);
	static optional<uint8_t> ParseMonitorMode(const string&);
	bool SetScreenDimensions(size_t width, size_t height);
	void ClearVideoState();
	void QueueSnapshot(bool);
	void ProcessSnapshots(stop_token);
	void WriteSnapshot(const snapshot_t&) const;

	transfer_t pending_transfer = transfer_t::none;
	size_t pending_transfer_length = 0;
	optional<framebuffer_update_t> pending_framebuffer_update;

	uint16_t screen_width = 640;
	uint16_t screen_height = 400;
	pixel_format_t pixel_format = pixel_format_t::one_bit;
	array<uint8_t, MAX_FRAMEBUFFER_BYTES> framebuffer {};
	array<color_t, 256> palette {};
	bool firmware_v21 = false;
	uint8_t monitor_mode = 0;
	uint8_t legacy_monitor_sense = 8;

	string snapshot_path;
	chrono::milliseconds snapshot_interval { 250 };
	chrono::steady_clock::time_point last_snapshot {};
	bool snapshot_full_refresh_only = false;
	mutex snapshot_mutex;
	condition_variable snapshot_condition;
	optional<snapshot_t> pending_snapshot;
	jthread snapshot_worker;

	// Retained for diagnostic use.
	vector<uint8_t> configuration_data;
	vector<uint8_t> palette_data;
	vector<uint8_t> unknown_cc_data;
};
