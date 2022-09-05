//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include "rascsi_version.h"
#include "os.h"
#include "log.h"
#include "rascsi_exceptions.h"
#include "device.h"

unordered_set<Device *> Device::devices;

Device::Device(const string& type)
{
	assert(type.length() == 4);

	devices.insert(this);

	this->type = type;

	char rev[5];
	sprintf(rev, "%02d%02d", rascsi_major_version, rascsi_minor_version);
	revision = rev;
}

Device::~Device()
{
	devices.erase(this);
}

void Device::Reset()
{
	locked = false;
	attn = false;
	reset = false;
}

void Device::SetProtected(bool b)
{
	if (!read_only) {
		write_protected = b;
	}
}

void Device::SetVendor(const string& v)
{
	if (v.empty() || v.length() > 8) {
		throw illegal_argument_exception("Vendor '" + v + "' must be between 1 and 8 characters");
	}

	vendor = v;
}

void Device::SetProduct(const string& p, bool force)
{
	if (p.empty() || p.length() > 16) {
		throw illegal_argument_exception("Product '" + p + "' must be between 1 and 16 characters");
	}

	// Changing the device name is not SCSI compliant
	if (!product.empty() && !force) {
		return;
	}

	product = p;
}

void Device::SetRevision(const string& r)
{
	if (r.empty() || r.length() > 4) {
		throw illegal_argument_exception("Revision '" + r + "' must be between 1 and 4 characters");
	}

	revision = r;
}

string Device::GetPaddedName() const
{
	string name = vendor;
	name.append(8 - vendor.length(), ' ');
	name += product;
	name.append(16 - product.length(), ' ');
	name += revision;
	name.append(4 - revision.length(), ' ');

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

void Device::SetStatusCode(int s)
{
	if (s) {
		LOGDEBUG("Error status: Sense Key $%02X, ASC $%02X, ASCQ $%02X", s >> 16, (s >> 8 &0xff), s & 0xff)
	}

	status_code = s;
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
