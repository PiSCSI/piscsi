//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "rascsi_interface.pb.h"
#include "protobuf_serializer.h"
#include "protobuf_util.h"
#include <sstream>

using namespace std;
using namespace rascsi_interface;

#define FPRT(fp, ...) fprintf(fp, __VA_ARGS__ )

static const char COMPONENT_SEPARATOR = ':';
static const char KEY_VALUE_SEPARATOR = '=';

void protobuf_util::ParseParameters(PbDeviceDefinition& device, const string& params)
{
	if (params.empty()) {
		return;
	}

	// Old style parameters, for backwards compatibility only.
	// Only one of these parameters will be used by rascsi, depending on the device type.
	if (params.find(KEY_VALUE_SEPARATOR) == string::npos) {
		SetParam(device, "file", params);
		if (params != "bridge" && params != "daynaport" && params != "printer" && params != "services") {
			SetParam(device, "interfaces", params);
		}

		return;
	}

	stringstream ss(params);
	string p;
	while (getline(ss, p, COMPONENT_SEPARATOR)) {
		if (!p.empty()) {
			const size_t separator_pos = p.find(KEY_VALUE_SEPARATOR);
			if (separator_pos != string::npos) {
				SetParam(device, p.substr(0, separator_pos), string_view(p).substr(separator_pos + 1));
			}
		}
	}
}

string protobuf_util::GetParam(const PbCommand& command, const string& key)
{
	const auto& it = command.params().find(key);
	return it != command.params().end() ? it->second : "";
}

string protobuf_util::GetParam(const PbDeviceDefinition& device, const string& key)
{
	const auto& it = device.params().find(key);
	return it != device.params().end() ? it->second : "";
}

void protobuf_util::SetParam(PbCommand& command, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *command.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::SetParam(PbDevice& device, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::SetParam(PbDeviceDefinition& device, const string& key, string_view value)
{
	if (!key.empty() && !value.empty()) {
		auto& map = *device.mutable_params();
		map[key] = value;
	}
}

void protobuf_util::SetPatternParams(PbCommand& command, string_view patterns)
{
	string folder_pattern;
	string file_pattern;
	if (const size_t separator_pos = patterns.find(COMPONENT_SEPARATOR); separator_pos != string::npos) {
		folder_pattern = patterns.substr(0, separator_pos);
		file_pattern = patterns.substr(separator_pos + 1);
	}
	else {
		file_pattern = patterns;
	}

	SetParam(command, "folder_pattern", folder_pattern);
	SetParam(command, "file_pattern", file_pattern);
}
