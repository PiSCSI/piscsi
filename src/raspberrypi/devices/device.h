//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_set>
#include <unordered_map>
#include <string>

using namespace std;

#define DEFAULT_VENDOR "RaSCSI"

class Device
{
	string type;

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

	// The block size is configurable
	bool block_size_configurable = false;

	// Device can be created with parameters
	bool supports_params = false;

	// Device ID and LUN
	int32_t id = 0;
	int32_t lun = 0;

	// Device identifier (for INQUIRY)
	string vendor;
	string product;
	string revision;

	// The parameters the device was created with
	unordered_map<string, string> params;

	// The default parameters
	unordered_map<string, string> default_params;

	// Sense Key, ASC and ASCQ
	//	MSB		Reserved (0x00)
	//			Sense Key
	//			Additional Sense Code (ASC)
	//	LSB		Additional Sense Code Qualifier(ASCQ)
	int status_code = scsi_defs::status::GOOD;

protected:

	void SetReady(bool ready) { this->ready = ready; }
	bool IsReset() const { return reset; }
	void SetReset(bool reset) { this->reset = reset; }
	bool IsAttn() const { return attn; }
	void SetAttn(bool attn) { this->attn = attn; }

	int GetStatusCode() const { return status_code; }

	const string GetParam(const string&);
	void SetParams(const unordered_map<string, string>&);

	static unordered_set<Device *> devices;

public:

	Device(const string&);
	virtual ~Device();

	// Override for device specific initializations, to be called after all device properties have been set
	virtual bool Init(const unordered_map<string, string>&) { return true; };

	virtual bool Dispatch() = 0;

	const string& GetType() const { return type; }

	bool IsReady() const { return ready; }
	void Reset();

	bool IsProtectable() const { return protectable; }
	void SetProtectable(bool protectable) { this->protectable = protectable; }
	bool IsProtected() const { return write_protected; }
	void SetProtected(bool);
	bool IsReadOnly() const { return read_only; }
	void SetReadOnly(bool read_only) { this->read_only = read_only; }

	bool IsStoppable() const { return stoppable; }
	void SetStoppable(bool stoppable) { this->stoppable = stoppable; }
	bool IsStopped() const { return stopped; }
	void SetStopped(bool stopped) { this->stopped = stopped; }
	bool IsRemovable() const { return removable; }
	void SetRemovable(bool removable) { this->removable = removable; }
	bool IsRemoved() const { return removed; }
	void SetRemoved(bool removed) { this->removed = removed; }

	bool IsLockable() const { return lockable; }
	void SetLockable(bool lockable) { this->lockable = lockable; }
	bool IsLocked() const { return locked; }
	void SetLocked(bool locked) { this->locked = locked; }

	int32_t GetId() const { return id; }
	void SetId(int32_t id) { this->id = id; }
	int32_t GetLun() const { return lun; }
	void SetLun(int32_t lun) { this->lun = lun; }

	const string GetVendor() const { return vendor; }
	void SetVendor(const string&);
	const string GetProduct() const { return product; }
	void SetProduct(const string&, bool = true);
	const string GetRevision() const { return revision; }
	void SetRevision(const string&);
	const string GetPaddedName() const;

	bool SupportsParams() const { return supports_params; }
	virtual bool SupportsFile() const { return !supports_params; }
	void SupportsParams(bool supports_paams) { this->supports_params = supports_paams; }
	const unordered_map<string, string> GetParams() const { return params; }
	void SetDefaultParams(const unordered_map<string, string>& default_params) { this->default_params = default_params; }

	void SetStatusCode(int);

	bool Start();
	void Stop();
	virtual bool Eject(bool);
	virtual void FlushCache() { }
};
