//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rascsi_version.h"
#include "log.h"
#include "device.h"
#include <cassert>
#include <sstream>
#include <iomanip>

using namespace std;

Device::Device(const string& type, int lun) : type(type), lun(lun)
{
	assert(type.length() == 4);

	ostringstream os;
	os << setw(2) << setfill('0') << rascsi_major_version << setw(2) << setfill('0') << rascsi_minor_version;
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
		throw invalid_argument("Vendor '" + v + "' must be between 1 and 8 characters");
	}

	vendor = v;
}

void Device::SetProduct(const string& p, bool force)
{
	if (p.empty() || p.length() > 16) {
		throw invalid_argument("Product '" + p + "' must be between 1 and 16 characters");
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
		throw invalid_argument("Revision '" + r + "' must be between 1 and 4 characters");
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

	for (const auto& [key, value] : set_params) {
		// It is assumed that there are default parameters for all supported parameters
		if (params.find(key) != params.end()) {
			params[key] = value;
		}
		else {
			LOGWARN("%s", string("Ignored unknown parameter '" + key + "'").c_str())
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
