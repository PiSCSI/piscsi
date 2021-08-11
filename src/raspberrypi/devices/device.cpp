//---------------------------------------------------------------------------
//
//      SCSI Target Emulator RaSCSI (*^..^*)
//      for Raspberry Pi
//
//      Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <cassert>
#include "device.h"

Device::Device(std::string id, bool removable)
{
	assert(id.length() == 4);

	this->id = id;
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
