//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "shared/piscsi_version.h"
#include "device.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <sstream>
#include <iomanip>
#include <stdexcept>

using namespace std;

Device::Device(PbDeviceType type, int lun) : type(type), lun(lun)
{
	ostringstream os;
	os << setfill('0') << setw(2) << piscsi_major_version << setw(2) << piscsi_minor_version;
	revision = os.str();
}

void Device::Reset()
{
	locked = false;
	attn = false;
	reset = false;
}

void Device::SetProtected(bool b)
{
	if (protectable && !read_only) {
		write_protected = b;
	}
}

void Device::SetVendor(const string& v)
{
	if (v.empty() || v.length() > 8) {
		throw invalid_argument("Vendor '" + v + "' must have between 1 and 8 characters");
	}

	vendor = v;
}

void Device::SetProduct(const string& p, bool force)
{
	if (p.empty() || p.length() > 16) {
		throw invalid_argument("Product '" + p + "' must have between 1 and 16 characters");
	}

	// Changing vital product data is not SCSI compliant
	if (!product.empty() && !force) {
		return;
	}

	product = p;
}

void Device::SetRevision(const string& r)
{
	if (r.empty() || r.length() > 4) {
		throw invalid_argument("Revision '" + r + "' must have between 1 and 4 characters");
	}

	revision = r;
}

string Device::GetPaddedName() const
{
	ostringstream os;
	os << left << setfill(' ') << setw(8) << vendor << setw(16) << product << setw(4) << revision;

	const string name = os.str();
	assert(name.length() == 28);

	return name;
}

string Device::GetParam(const string& key) const
{
	const auto& it = params.find(key);
	return it == params.end() ? "" : it->second;
}

void Device::SetParams(const unordered_map<string, string>& set_params)
{
	params = default_params;

	// Devices with image file support implicitly support the "file" parameter
	if (SupportsFile()) {
		params["file"] = "";
	}

	for (const auto& [key, value] : set_params) {
		// It is assumed that there are default parameters for all supported parameters
		if (params.contains(key)) {
			params[key] = value;
		}
		else {
			spdlog::warn("Ignored unknown parameter '" + key + "'");
		}
	}
}

bool Device::Start()
{
	if (!ready) {
		return false;
	}

	stopped = false;

	return true;
}

void Device::Stop()
{
	ready = false;
	attn = false;
	stopped = true;

	status_code = 0;
}

bool Device::Eject(bool force)
{
	if (!ready || !removable) {
		return false;
	}

	// Must be unlocked if there is no force flag
	if (!force && locked) {
		return false;
	}

	ready = false;
	attn = false;
	removed = true;
	write_protected = false;
	locked = false;
	stopped = true;

	return true;
}
