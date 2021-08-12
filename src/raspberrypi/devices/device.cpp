//---------------------------------------------------------------------------
//
//      SCSI Target Emulator RaSCSI (*^..^*)
//      for Raspberry Pi
//
//      Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include <sstream>
#include "rascsi_version.h"
#include "exceptions.h"
#include "device.h"

Device::Device(const string& type, bool removable)
{
	assert(type.length() == 4);

	vendor = DEFAULT_VENDOR;
	char rev[5];
	sprintf(rev, "%02d%02d", rascsi_major_version, rascsi_minor_version);
	revision = rev;

	this->type = type;
	this->removable = removable;

	ready = false;
	reset = false;
	attn = false;
	protectable = false;
	write_protected = false;
	read_only = false;
	removed = false;
	lockable = false;
	locked = false;

	id = 0;
	lun = 0;

	status_code = STATUS_NOERROR;
}

void Device::Reset()
{
	locked = false;
	attn = false;
	reset = false;
}

bool Device::SetProtected(bool write_protected)
{
	if (!ready) {
		return false;
	}

	// Read Only, protect only
	if (read_only && !write_protected) {
		return false;
	}

	this->write_protected = write_protected;

	return true;
}

void Device::SetVendor(const string& vendor)
{
	if (vendor.empty() || vendor.length() > 8) {
		ostringstream error;
		error << "Vendor '" << vendor << "' must be between 1 and 8 characters";
		throw illegalargumentexception(error.str());
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
		ostringstream error;
		error << "Product '" << product << "' must be between 1 and 16 characters";
		throw illegalargumentexception(error.str());
	}

	this->product = product;
}

void Device::SetRevision(const string& revision)
{
	if (revision.empty() || revision.length() > 4) {
		ostringstream error;
		error << "Revision '" << revision << "' must be between 1 and 4 characters";
		throw illegalargumentexception(error.str());
	}

	this->revision = revision;
}

void Device::GetPaddedName(string& name) const
{
	name = vendor;
	name.append(8 - vendor.length(), ' ');
	name += product;
	name.append(16 - product.length(), ' ');
	name += revision;
	name.append(4 - revision.length(), ' ');

	assert(name.length() == 28);
}

// TODO This implementation appears to be wrong: If a device is locked there
// is no way to eject the medium without unlocking. In other words, there is
// no "force" mode.
bool Device::Eject(bool force)
{
	if (!IsReady() || !IsRemovable()) {
		return false;
	}

	// Must be unlocked if there is no force flag
	if (!force && IsLocked()) {
		return false;
	}

	SetReady(false);
	SetProtected(false);
	SetReadOnly(false);
	SetRemoved(true);
	SetAttn(false);

	return true;
}
