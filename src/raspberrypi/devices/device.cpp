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

	vendor = DEFAULT_VENDOR;
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

void Device::SetProtected(bool write_protected)
{
	if (!read_only) {
		this->write_protected = write_protected;
	}
}

void Device::SetVendor(const string& vendor)
{
	if (vendor.empty() || vendor.length() > 8) {
		throw illegal_argument_exception("Vendor '" + vendor + "' must be between 1 and 8 characters");
	}

	this->vendor = vendor;
}

void Device::SetProduct(const string& product, bool force)
{
	// Changing the device name is not SCSI compliant
	if (!this->product.empty() && !force) {
		return;
	}

	if (product.empty() || product.length() > 16) {
		throw illegal_argument_exception("Product '" + product + "' must be between 1 and 16 characters");
	}

	this->product = product;
}

void Device::SetRevision(const string& revision)
{
	if (revision.empty() || revision.length() > 4) {
		throw illegal_argument_exception("Revision '" + revision + "' must be between 1 and 4 characters");
	}

	this->revision = revision;
}

const string Device::GetPaddedName() const
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

const string Device::GetParam(const string& key)
{
	return params.find(key) != params.end() ? params[key] : "";
}

void Device::SetParams(const unordered_map<string, string>& set_params)
{
	params = default_params;

	for (const auto& param : set_params) {
		// It is assumed that there are default parameters for all supported parameters
		if (params.find(param.first) != params.end()) {
			params[param.first] = param.second;
		}
		else {
			LOGWARN("%s", string("Ignored unknown parameter '" + param.first + "'").c_str());
		}
	}
}

void Device::SetStatusCode(int status_code)
{
	if (status_code) {
		LOGDEBUG("Error status: Sense Key $%02X, ASC $%02X, ASCQ $%02X", status_code >> 16, (status_code >> 8 &0xff), status_code & 0xff);
	}

	this->status_code = status_code;
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
