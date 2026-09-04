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

#include "shared/piscsi_exceptions.h"
#include "shared/piscsi_util.h"
#include "scsi_powerview.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace scsi_defs;
using namespace piscsi_util;

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

constexpr array<SCSIPowerView::mode_descriptor_t, 7> monitor_modes = {{
	{ 0,  640, 480 },
	{ 1,  624, 840 },
	{ 2,  800, 600 },
	{ 3,  640, 400 },
	{ 4,  624, 840 },
	{ 5,  640, 400 },
	{ 6, 1152, 882 }
}};

string FormatBytes(span<const uint8_t> data, size_t maximum = 16)
{
	ostringstream result;
	result << hex << setfill('0');
	for (size_t i = 0; i < min(data.size(), maximum); ++i) {
		if (i) {
			result << ' ';
		}
		result << setw(2) << static_cast<unsigned int>(data[i]);
	}
	if (data.size() > maximum) {
		result << " ...";
	}
	return result.str();
}

}

SCSIPowerView::SCSIPowerView(int lun) : PrimaryDevice(SCPV, lun)
{
	SupportsParams(true);
	ClearVideoState();
	SetReady(true);
}

SCSIPowerView::~SCSIPowerView()
{
	if (snapshot_worker.joinable()) {
		snapshot_worker.request_stop();
		snapshot_condition.notify_one();
	}
}

bool SCSIPowerView::Init(const param_map& params)
{
	if (!PrimaryDevice::Init(params)) {
		return false;
	}

	if (int interval; !GetAsUnsignedInt(GetParam("snapshot_interval"), interval)) {
		LogError("Invalid PowerView snapshot interval");
		return false;
	}
	else {
		snapshot_interval = chrono::milliseconds(interval);
	}

	const string revision = GetParam("firmware_revision");
	if (revision == "1.0") {
		firmware_v21 = false;
	}
	else if (revision == "2.1") {
		firmware_v21 = true;
	}
	else {
		LogError("Invalid PowerView firmware_revision");
		return false;
	}

	const auto configured_mode = ParseMonitorMode(GetParam("monitor_mode"));
	if (!configured_mode) {
		LogError("Invalid PowerView monitor_mode");
		return false;
	}
	monitor_mode = *configured_mode;

	if (int sense; !GetAsUnsignedInt(GetParam("legacy_monitor_sense"), sense) || sense < 0 || sense > 8) {
		LogError("Invalid PowerView legacy_monitor_sense");
		return false;
	}
	else {
		legacy_monitor_sense = static_cast<uint8_t>(sense);
	}

	const string full_refresh_only = GetParam("snapshot_full_refresh_only");
	if (full_refresh_only == "true") {
		snapshot_full_refresh_only = true;
	}
	else if (full_refresh_only == "false") {
		snapshot_full_refresh_only = false;
	}
	else {
		LogError("Invalid PowerView snapshot_full_refresh_only setting");
		return false;
	}

	snapshot_path = GetParam("snapshot");
	if (!snapshot_path.empty()) {
		const filesystem::path path(snapshot_path);
		error_code error;
		if (!path.parent_path().empty() && !filesystem::is_directory(path.parent_path(), error)) {
			LogError("Invalid PowerView snapshot path '" + snapshot_path + "'");
			return false;
		}
		if (error || (filesystem::exists(path, error) && filesystem::is_directory(path, error)) || error) {
			LogError("Invalid PowerView snapshot path '" + snapshot_path + "'");
			return false;
		}
	}
	if (!snapshot_path.empty()) {
		snapshot_worker = jthread([this] (stop_token stop) { ProcessSnapshots(stop); });
	}

	AddCommand(scsi_command::eCmdRead6, [this] { Read6(); });
	AddCommand(scsi_command::eCmdPowerViewV21ReadMode, [this] { ReadV21MonitorMode(); });
	AddCommand(scsi_command::eCmdPowerViewV21Write, [this] { WriteV21Handshake(); });
	AddCommand(scsi_command::eCmdPowerViewV21ModeSet, [this] { V21ModeSet(); });
	AddCommand(scsi_command::eCmdPowerViewReadConfig, [this] { ReadConfiguration(); });
	AddCommand(scsi_command::eCmdPowerViewWriteConfig, [this] { WriteConfiguration(); });
	AddCommand(scsi_command::eCmdPowerViewWriteFrameBuffer, [this] { WriteFrameBuffer(); });
	AddCommand(scsi_command::eCmdPowerViewWriteColorPalette, [this] { WriteColorPalette(); });
	AddCommand(scsi_command::eCmdPowerViewUnknownCC, [this] { WriteUnknownCC(); });
	AddCommand(scsi_command::eCmdPowerViewQuadraSetup, [this] { QuadraSetup(); });
	ClearVideoState();

	return true;
}

param_map SCSIPowerView::GetDefaultParams() const
{
	return {
		{ "snapshot", "" },
		{ "snapshot_interval", "250" },
		{ "snapshot_full_refresh_only", "false" },
		{ "firmware_revision", "1.0" },
		{ "monitor_mode", "640x480" },
		{ "legacy_monitor_sense", "8" }
	};
}

void SCSIPowerView::Reset()
{
	pending_transfer = transfer_t::none;
	pending_transfer_length = 0;
	pending_framebuffer_update.reset();
	ClearVideoState();

	PrimaryDevice::Reset();
}

vector<uint8_t> SCSIPowerView::InquiryInternal() const
{
	auto response = vector<uint8_t>(inquiry_response.begin(), inquiry_response.end());
	if (firmware_v21) {
		response[32] = 'V';
		response[33] = '2';
		response[34] = '.';
		response[35] = '1';
	}
	return response;
}

void SCSIPowerView::Read6() const
{
	// RadiusWare probes the PowerView processor target with READ(6) during
	// startup. The legacy card reports that probe as an empty medium; returning
	// INVALID COMMAND makes the SE/30 driver retry and abandon initialization.
	throw scsi_exception(sense_key::not_ready, asc::medium_not_present);
}

void SCSIPowerView::ReadConfiguration()
{
	const size_t length = static_cast<uint8_t>(GetController()->GetCmdByte(6));
	LogDebug("PowerView: C8 configuration read, selector " + to_string(GetController()->GetCmdByte(3)) + ":"
			+ to_string(GetController()->GetCmdByte(4)) + ", " + to_string(length) + " byte(s)");
	GetController()->AllocateBuffer(length);

	auto& buffer = GetController()->GetBuffer();
	fill_n(buffer.begin(), length, 0);

	if (GetController()->GetCmdByte(3) == 0x31) {
		switch (GetController()->GetCmdByte(4)) {
		case 0x00: {
			array<uint8_t, 3> response = { 0x01, 0x09, 0x08 };
			if (firmware_v21) {
				// Sense code 7 selects RadiusWare's enhanced C4/C3/C2 path.
				response = { 0x09, 0x09, 0x09 };
			}
			else if (legacy_monitor_sense != 8) {
				response = { static_cast<uint8_t>(0x08 | (legacy_monitor_sense & 1)),
						static_cast<uint8_t>(0x08 | ((legacy_monitor_sense >> 1) & 1)),
						static_cast<uint8_t>(0x08 | ((legacy_monitor_sense >> 2) & 1)) };
			}
			copy_n(response.begin(), min(response.size(), length), buffer.begin());
			break;
		}

		case 0x82:
			if (length) {
				buffer[0] = 0x01;
			}
			break;

		case 0x83:
			// A clear bit forces legacy extended sense. V2.1 needs the base
			// sense value 7 so RadiusWare performs the enhanced handshake.
			if (firmware_v21 || legacy_monitor_sense != 8) {
				if (length) {
					buffer[0] = 0x01;
				}
			}
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

void SCSIPowerView::ReadV21MonitorMode()
{
	if (!firmware_v21) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
	}
	GetController()->AllocateBuffer(1);
	GetController()->GetBuffer()[0] = monitor_mode;
	GetController()->SetBlocks(1);
	GetController()->SetLength(1);
	EnterDataInPhase();
}

void SCSIPowerView::WriteV21Handshake()
{
	if (!firmware_v21) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
	}
	StartDataOut(transfer_t::v21_write, 1);
}

void SCSIPowerView::V21ModeSet()
{
	if (!firmware_v21) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
	}
	const int operation = GetController()->GetCmdByte(1);
	if (operation != 0 && operation != 1) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}
	EnterStatusPhase();
}

void SCSIPowerView::QuadraSetup()
{
	if (!firmware_v21) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
	}
	EnterStatusPhase();
}

void SCSIPowerView::WriteConfiguration()
{
	const size_t length = static_cast<uint8_t>(GetController()->GetCmdByte(6));
	LogDebug("PowerView: C9 configuration write, selector " + to_string(GetController()->GetCmdByte(3)) + ":"
			+ to_string(GetController()->GetCmdByte(4)) + ", " + to_string(length) + " byte(s)");
	if (!length) {
		EnterStatusPhase();
		return;
	}

	StartDataOut(transfer_t::configuration, length);
}

void SCSIPowerView::WriteFrameBuffer()
{
	const size_t offset = (static_cast<size_t>(GetController()->GetCmdByte(1)) << 16) |
			(static_cast<size_t>(GetController()->GetCmdByte(2)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(3));
	const size_t width_bytes = (static_cast<size_t>(GetController()->GetCmdByte(4)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(5));
	const size_t height = (static_cast<size_t>(GetController()->GetCmdByte(6)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(7));

	if (!width_bytes || !height || width_bytes > MAX_FRAMEBUFFER_BYTES / height) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	const size_t pixels_per_byte = pixel_format == pixel_format_t::one_bit ? 8 :
			pixel_format == pixel_format_t::four_bit ? 2 : 1;
	// CA is an 11-byte CDB. The legacy implementation (and RadiusWare) use
	// byte 9 to distinguish a complete refresh from an update at offset zero.
	if (!offset && !GetController()->GetCmdByte(9)) {
		if (firmware_v21) {
			const auto& mode = GetActiveMode();
			if (width_bytes != mode.width / pixels_per_byte || height != mode.height) {
				throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
			}
		}
		else if (!SetScreenDimensions(width_bytes * pixels_per_byte, height)) {
			throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
		}
	}

	const auto update = GetFrameBufferUpdate();
	if (!update) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}
	LogDebug("PowerView: CA framebuffer write, offset " + to_string(offset) + ", " + to_string(width_bytes) + " by "
			+ to_string(height) + " packed bytes/pixels");

	StartDataOut(transfer_t::framebuffer, width_bytes * height);
	pending_framebuffer_update = update;
}

void SCSIPowerView::WriteColorPalette()
{
	const size_t entries = (static_cast<size_t>(GetController()->GetCmdByte(3)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(4));
	if (!entries) {
		// A zero depth has no payload and does not change the currently latched
		// pixel format.
		EnterStatusPhase();
		return;
	}

	// The depth code is also the number of 32-bit palette entries on the
	// wire. In particular, CB 00 00 01 transfers four bytes, not eight.
	const size_t length = entries * 4;
	LogDebug("PowerView: CB palette write, " + to_string(entries) + " entries, " + to_string(length) + " byte(s)");
	if (entries == 1 || entries == 2) {
		pixel_format = pixel_format_t::one_bit;
	}
	else if (entries == 16) {
		pixel_format = pixel_format_t::four_bit;
	}
	else if (entries == 256) {
		pixel_format = pixel_format_t::eight_bit;
	}
	StartDataOut(transfer_t::palette, length);
}

void SCSIPowerView::WriteUnknownCC()
{
	// The CDB stores a bit count in bytes 1:2. RadiusWare transfers the
	// corresponding byte count, which is not always the historical 0x8bb.
	const size_t length = ((static_cast<size_t>(GetController()->GetCmdByte(1)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(2))) >> 3;
	if (!length) {
		EnterStatusPhase();
		return;
	}
	LogDebug("PowerView: CC script write, " + to_string(length) + " byte(s)");

	StartDataOut(transfer_t::unknown_cc, length);
}

void SCSIPowerView::StartDataOut(transfer_t transfer, size_t length)
{
	pending_framebuffer_update.reset();
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
		LogDebug("PowerView: C9 payload " + FormatBytes(data));
		break;

	case transfer_t::v21_write:
		LogDebug("PowerView: C3 V2.1 handshake payload " + FormatBytes(data));
		break;

	case transfer_t::palette:
		palette_data.assign(data.begin(), data.end());
		LogDebug("PowerView: CB payload " + FormatBytes(data));
		ApplyPalette(data);
		break;

	case transfer_t::unknown_cc:
		unknown_cc_data.assign(data.begin(), data.end());
		break;

	case transfer_t::framebuffer:
		if (!pending_framebuffer_update) {
			LogWarn("Missing PowerView framebuffer update state");
			return false;
		}
		ApplyFrameBufferUpdate(data);
		break;

	case transfer_t::none:
		return false;
	}

	pending_transfer = transfer_t::none;
	pending_transfer_length = 0;
	pending_framebuffer_update.reset();
	return true;
}

void SCSIPowerView::ApplyFrameBufferUpdate(span<const uint8_t> data)
{
	const auto& update = *pending_framebuffer_update;
	const size_t nonzero_bytes = count_if(data.begin(), data.end(), [] (uint8_t byte) { return byte != 0; });
	LogDebug("PowerView: CA payload has " + to_string(nonzero_bytes) + " nonzero packed byte(s) of " + to_string(data.size()));
	for (size_t y = 0; y < update.height; ++y) {
		for (size_t x = 0; x < update.width_pixels; ++x) {
			uint8_t color_index = 0;
			switch (pixel_format) {
			case pixel_format_t::one_bit:
				// PowerView stores the leftmost pixel in the most-significant bit.
				// Its monochrome palette is addressed as 0x00 (clear/white) and
				// 0x80 (set/black), rather than as palette slots 0 and 1.
				color_index = (data[y * update.width_bytes + x / 8] >> (7 - (x % 8))) & 1 ? 0x80 : 0x00;
				break;

			case pixel_format_t::four_bit: {
				const uint8_t packed = data[y * update.width_bytes + x / 2];
				color_index = x % 2 ? packed & 0x0f : packed >> 4;
				break;
			}

			case pixel_format_t::eight_bit:
				color_index = data[y * update.width_bytes + x];
				break;
			}

			framebuffer[(update.row + y) * MAX_WIDTH + update.column + x] = color_index;
		}
	}

	QueueSnapshot(update.full_refresh);
}

void SCSIPowerView::ApplyPalette(span<const uint8_t> data)
{
	for (size_t i = 0; i < data.size(); i += 4) {
		palette[data[i]] = { data[i + 1], data[i + 2], data[i + 3] };
	}
}

optional<SCSIPowerView::framebuffer_update_t> SCSIPowerView::GetFrameBufferUpdate() const
{
	const size_t address = (static_cast<size_t>(GetController()->GetCmdByte(1)) << 16) |
			(static_cast<size_t>(GetController()->GetCmdByte(2)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(3));
	const size_t width_bytes = (static_cast<size_t>(GetController()->GetCmdByte(4)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(5));
	const size_t height = (static_cast<size_t>(GetController()->GetCmdByte(6)) << 8) |
			static_cast<uint8_t>(GetController()->GetCmdByte(7));

	const size_t pixels_per_byte = pixel_format == pixel_format_t::one_bit ? 8 :
			pixel_format == pixel_format_t::four_bit ? 2 : 1;
	if (!height || (pixel_format != pixel_format_t::eight_bit && address % 2) ||
			width_bytes > MAX_FRAMEBUFFER_BYTES / height) {
		return nullopt;
	}

	const size_t byte_offset = pixel_format == pixel_format_t::eight_bit ? address : address / 2;
	const size_t bytes_per_row = screen_width / pixels_per_byte;
	const size_t row = byte_offset / bytes_per_row;
	const size_t column = (byte_offset % bytes_per_row) * pixels_per_byte;
	const size_t width_pixels = width_bytes * pixels_per_byte;
	if (!width_bytes || !height || row >= screen_height || column >= screen_width ||
			width_pixels > screen_width - column || height > screen_height - row) {
		return nullopt;
	}

	const bool full_refresh = address == 0 && !GetController()->GetCmdByte(9);
	return framebuffer_update_t { width_bytes, height, row, column, width_pixels, full_refresh };
}

bool SCSIPowerView::SetScreenDimensions(size_t width, size_t height)
{
	const auto mode = find_if(monitor_modes.begin(), monitor_modes.end(), [width, height] (const auto& candidate) {
		return candidate.width == width && candidate.height == height;
	});
	if (mode == monitor_modes.end()) {
		return false;
	}

	if (screen_width != width || screen_height != height) {
		screen_width = static_cast<uint16_t>(width);
		screen_height = static_cast<uint16_t>(height);
		framebuffer.fill(0);
	}
	return true;
}

const SCSIPowerView::mode_descriptor_t& SCSIPowerView::GetActiveMode() const
{
	const auto mode = GetMode(monitor_mode);
	assert(mode);
	return *find_if(monitor_modes.begin(), monitor_modes.end(), [mode] (const auto& candidate) {
		return candidate.code == mode->code;
	});
}

optional<SCSIPowerView::mode_descriptor_t> SCSIPowerView::GetMode(uint8_t code)
{
	const auto mode = find_if(monitor_modes.begin(), monitor_modes.end(), [code] (const auto& candidate) {
		return candidate.code == code;
	});
	return mode == monitor_modes.end() ? nullopt : optional<mode_descriptor_t>(*mode);
}

optional<uint8_t> SCSIPowerView::ParseMonitorMode(const string& value)
{
	if (value.size() == 1 && value[0] >= '0' && value[0] <= '6') {
		return static_cast<uint8_t>(value[0] - '0');
	}
	for (const auto& mode : monitor_modes) {
		if (value == to_string(mode.width) + "x" + to_string(mode.height)) {
			return mode.code;
		}
	}
	return nullopt;
}

void SCSIPowerView::ClearVideoState()
{
	const auto& mode = firmware_v21 ? GetActiveMode() : mode_descriptor_t { 3, 640, 400 };
	screen_width = mode.width;
	screen_height = mode.height;
	pixel_format = pixel_format_t::one_bit;
	framebuffer.fill(0);
	palette.fill({});
	// Permit a useful monochrome image even if the first framebuffer write
	// arrives before the first palette update.
	palette[0x00] = { 0xff, 0xff, 0xff };
	palette[0x80] = { 0x00, 0x00, 0x00 };
}

void SCSIPowerView::QueueSnapshot(bool full_refresh)
{
	if (snapshot_path.empty() || (snapshot_full_refresh_only && !full_refresh)) {
		return;
	}

	const auto now = chrono::steady_clock::now();
	if (last_snapshot != chrono::steady_clock::time_point {} && now - last_snapshot < snapshot_interval) {
		return;
	}

	snapshot_t snapshot { screen_width, screen_height, {}, palette };
	snapshot.pixels.reserve(static_cast<size_t>(screen_width) * screen_height);
	for (size_t y = 0; y < screen_height; ++y) {
		const auto row_start = framebuffer.begin() + static_cast<ptrdiff_t>(y * MAX_WIDTH);
		snapshot.pixels.insert(snapshot.pixels.end(), row_start, row_start + screen_width);
	}

	{
		const lock_guard<mutex> lock(snapshot_mutex);
		// Only the newest image is useful. Replacing an unprocessed job bounds
		// memory use and prevents rendering from falling behind SCSI traffic.
		pending_snapshot = std::move(snapshot);
	}
	snapshot_condition.notify_one();
	last_snapshot = now;
}

void SCSIPowerView::ProcessSnapshots(stop_token stop)
{
	while (!stop.stop_requested()) {
		optional<snapshot_t> snapshot;
		{
			unique_lock<mutex> lock(snapshot_mutex);
			snapshot_condition.wait(lock, [this, &stop] { return stop.stop_requested() || pending_snapshot.has_value(); });
			if (stop.stop_requested()) {
				return;
			}
			snapshot = std::move(pending_snapshot);
			pending_snapshot.reset();
		}

		WriteSnapshot(*snapshot);
	}
}

void SCSIPowerView::WriteSnapshot(const snapshot_t& snapshot) const
{
	vector<uint8_t> rgb;
	rgb.reserve(snapshot.pixels.size() * 3);
	for (const uint8_t pixel : snapshot.pixels) {
		const auto& color = snapshot.palette[pixel];
		rgb.insert(rgb.end(), color.begin(), color.end());
	}

	const filesystem::path target(snapshot_path);
	const filesystem::path temporary = target.string() + ".tmp";
	bool write_failed = false;
	{
		ofstream output(temporary, ios::binary | ios::trunc);
		if (!output) {
			LogWarn("Unable to open PowerView snapshot '" + temporary.string() + "'");
		}
		else {
			output << "P6\n" << snapshot.width << ' ' << snapshot.height << "\n255\n";
			output.write(reinterpret_cast<const char*>(rgb.data()), static_cast<streamsize>(rgb.size()));
			if (!output) {
				LogWarn("Unable to write PowerView snapshot '" + temporary.string() + "'");
			}
		}
		write_failed = !output;
	}

	if (write_failed) {
		error_code error;
		filesystem::remove(temporary, error);
		return;
	}

	error_code error;
	filesystem::rename(temporary, target, error);
	if (error) {
		LogWarn("Unable to publish PowerView snapshot '" + target.string() + "'");
		filesystem::remove(temporary, error);
		return;
	}

}
