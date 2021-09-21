//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <map>
#include <string>

using namespace std;

#define DEFAULT_VENDOR "RaSCSI"

//---------------------------------------------------------------------------
//
//	Error definition (sense code returned by REQUEST SENSE)
//
//	MSB		Reserved (0x00)
//			Sense Key
//			Additional Sense Code (ASC)
//	LSB		Additional Sense Code Qualifier(ASCQ)
//
//---------------------------------------------------------------------------
#define STATUS_NOERROR		0x00000000	// NO ADDITIONAL SENSE INFO.
#define STATUS_DEVRESET		0x00062900	// POWER ON OR RESET OCCURED
#define STATUS_NOTREADY		0x00023a00	// MEDIUM NOT PRESENT
#define STATUS_ATTENTION	0x00062800	// MEDIUM MAY HAVE CHANGED
#define STATUS_PREVENT		0x00045302	// MEDIUM REMOVAL PREVENTED
#define STATUS_READFAULT	0x00031100	// UNRECOVERED READ ERROR
#define STATUS_WRITEFAULT	0x00030300	// PERIPHERAL DEVICE WRITE FAULT
#define STATUS_WRITEPROTECT	0x00042700	// WRITE PROTECTED
#define STATUS_MISCOMPARE	0x000e1d00	// MISCOMPARE DURING VERIFY
#define STATUS_INVALIDCMD	0x00052000	// INVALID COMMAND OPERATION CODE
#define STATUS_INVALIDLBA	0x00052100	// LOGICAL BLOCK ADDR. OUT OF RANGE
#define STATUS_INVALIDCDB	0x00052400	// INVALID FIELD IN CDB
#define STATUS_INVALIDLUN	0x00052500	// LOGICAL UNIT NOT SUPPORTED
#define STATUS_INVALIDPRM	0x00052600	// INVALID FIELD IN PARAMETER LIST
#define STATUS_INVALIDMSG	0x00054900	// INVALID MESSAGE ERROR
#define STATUS_PARAMLEN		0x00051a00	// PARAMETERS LIST LENGTH ERROR
#define STATUS_PARAMNOT		0x00052601	// PARAMETERS NOT SUPPORTED
#define STATUS_PARAMVALUE	0x00052602	// PARAMETERS VALUE INVALID
#define STATUS_PARAMSAVE	0x00053900	// SAVING PARAMETERS NOT SUPPORTED
#define STATUS_NODEFECT		0x00010000	// DEFECT LIST NOT FOUND

class SCSIDEV;

class Device
{
private:

	string type;

	bool ready;
	bool reset;
	bool attn;

	// Number of supported luns
	int supported_luns;

	// Device is protectable/write-protected
	bool protectable;
	bool write_protected;
	// Device is permanently read-only
	bool read_only;

	// Device can be stopped (parked)/is stopped (parked)
	bool stoppable;
	bool stopped;

	// Device is removable/removed
	bool removable;
	bool removed;

	// Device is lockable/locked
	bool lockable;
	bool locked;

	// The block size is configurable
	bool block_size_configurable;

	// Device can be created with parameters
	bool supports_params;

	// Device ID and LUN
	int32_t id;
	int32_t lun;

	// Device identifier (for INQUIRY)
	string vendor;
	string product;
	string revision;

	// The parameters the device was created with
	map<string, string> params;

	// The default parameters
	map<string, string> default_params;

	// Sense Key, ASC and ASCQ
	int status_code;

public:

	Device(const string&);
	virtual ~Device() {};

	// Override for device specific initializations, to be called after all device properties have been set
	virtual bool Init(const map<string, string>&) { return true; };

	virtual bool Dispatch(SCSIDEV *) = 0;

	const string& GetType() const { return type; }

	bool IsReady() const { return ready; }
	void SetReady(bool ready) { this->ready = ready; }
	bool IsReset() const { return reset; }
	void SetReset(bool reset) { this->reset = reset; }
	void Reset();
	bool IsAttn() const { return attn; }
	void SetAttn(bool attn) { this->attn = attn; }

	int GetSupportedLuns() const { return supported_luns; }
	void SetSupportedLuns(int supported_luns) { this->supported_luns = supported_luns; }

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
	void SupportsParams(bool supports_paams) { this->supports_params = supports_paams; }
	const map<string, string> GetParams() const { return params; }
	const string GetParam(const string&);
	void SetParams(const map<string, string>& params) { this->params = params; }
	const map<string, string> GetDefaultParams() const { return default_params; }
	void SetDefaultParams(const map<string, string>& default_params) { this->default_params = default_params; }

	int GetStatusCode() const { return status_code; }
	void SetStatusCode(int);

	bool Start();
	void Stop();
	virtual bool Eject(bool);

	bool IsSASIHD() const { return type == "SAHD"; }
	bool IsSCSIHD() const { return type == "SCHD" || type == "SCRM"; }
	bool IsCdRom() const { return type == "SCCD"; }
	bool IsMo() const { return type == "SCMO"; }
	bool IsBridge() const { return type == "SCBR"; }
	bool IsDaynaPort() const { return type == "SCDP"; }
};
