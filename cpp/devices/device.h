//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/piscsi_interface.pb.h"
#include "shared/piscsi_util.h"
#include <unordered_map>
#include <string>

using namespace std;
using namespace piscsi_interface;

// A combination of device ID and LUN
using id_set = pair<int, int>;

// The map used for storing/passing device parameters
using param_map = unordered_map<string, string, piscsi_util::StringHash>;

class Device //NOSONAR The number of fields and methods is justified, the complexity is low
{
	const string DEFAULT_VENDOR = "PiSCSI";

	PbDeviceType type;

	bool ready = false;
	bool reset = false;
	bool attn = false;

	// Device is protectable/write-protected
	bool protectable = false;
	bool write_protected = false;
	// Device is permanently read-only
	bool read_only = false;

	// Device can be stopped (parked)/is stopped (parked)
	bool stoppable = false;
	bool stopped = false;

	// Device is removable/removed
	bool removable = false;
	bool removed = false;

	// Device is lockable/locked
	bool lockable = false;
	bool locked = false;

	// A device can be created with parameters
	bool supports_params = false;

	// A device can support an image file
	bool supports_file = false;

	// Immutable LUN
	int lun;

	// Device identifier (for INQUIRY)
	string vendor = DEFAULT_VENDOR;
	string product;
	string revision;

	// The parameters the device was created with
	param_map params;

	// The default parameters
	param_map default_params;

	// Sense Key and ASC
	//	MSB		Reserved (0x00)
	//			Sense Key
	//			Additional Sense Code (ASC)
	int status_code = 0;

protected:

	Device(PbDeviceType, int);

	void SetReady(bool b) { ready = b; }
	bool IsReset() const { return reset; }
	void SetReset(bool b) { reset = b; }
	bool IsAttn() const { return attn; }
	void SetAttn(bool b) { attn = b; }

	void SetRemovable(bool b) { removable = b; }
	void SetStoppable(bool b) { stoppable = b; }
	void SetStopped(bool b) { stopped = b; }
	void SetLockable(bool b) { lockable = b; }
	void SetLocked(bool b) { locked = b; }

	int GetStatusCode() const { return status_code; }

	string GetParam(const string&) const;
	void SetParams(const param_map&);

public:

	virtual ~Device() = default;

	PbDeviceType GetType() const { return type; }
	string GetTypeString() const { return PbDeviceType_Name(type); }
	string GetIdentifier() const { return GetTypeString() + " " + to_string(GetId()) + ":" + to_string(lun); }

	bool IsReady() const { return ready; }
	virtual void Reset();

	bool IsProtectable() const { return protectable; }
	void SetProtectable(bool b) { protectable = b; }
	bool IsProtected() const { return write_protected; }
	void SetProtected(bool);
	bool IsReadOnly() const { return read_only; }
	void SetReadOnly(bool b) { read_only = b; }
	bool IsStoppable() const { return stoppable; }
	bool IsStopped() const { return stopped; }
	bool IsRemovable() const { return removable; }
	bool IsRemoved() const { return removed; }
	void SetRemoved(bool b) { removed = b; }
	bool IsLockable() const { return lockable; }
	bool IsLocked() const { return locked; }

	virtual int GetId() const = 0;
	int GetLun() const { return lun; }

	string GetVendor() const { return vendor; }
	void SetVendor(const string&);
	string GetProduct() const { return product; }
	void SetProduct(const string&, bool = true);
	string GetRevision() const { return revision; }
	void SetRevision(const string&);
	string GetPaddedName() const;

	bool SupportsParams() const { return supports_params; }
	bool SupportsFile() const { return supports_file; }
	void SupportsParams(bool b) { supports_params = b; }
	void SupportsFile(bool b) { supports_file = b; }
	auto GetParams() const { return params; }
	void SetDefaultParams(const param_map& p) { default_params = p; }

	void SetStatusCode(int s) { status_code = s; }

	bool Start();
	void Stop();
	virtual bool Eject(bool);
};
