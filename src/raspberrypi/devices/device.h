//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

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

class Device
{
private:

	string type;

	bool ready;
	bool reset;
	bool attn;

	// Device is protectable/write-protected
	bool protectable;
	bool write_protected;
	// Device is permanently read-only
	bool read_only;

	// Device is removable/removed
	bool removable;
	bool removed;

	// Device is lockable/locked
	bool lockable;
	bool locked;

	// Device ID and LUN
	unsigned int id;
	unsigned int lun;

	string vendor;
	string product;
	string revision;

	// Sense Key, ASC and ASCQ
	int status_code;

public:

	Device(const string&);
	virtual ~Device() {};

	const string& GetType() const { return type; }

	bool IsReady() const { return ready; }
	void SetReady(bool ready) { this->ready = ready; }
	bool IsReset() const { return reset; }
	void SetReset(bool reset) { this->reset = reset; }
	void Reset();
	bool IsAttn() const { return attn; }
	void SetAttn(bool attn) { this->attn = attn; }

	bool IsProtectable() const { return protectable; }
	void SetProtectable(bool protectable) { this->protectable = protectable; }
	bool IsProtected() const { return write_protected; }
	void SetProtected(bool);
	bool IsReadOnly() const { return read_only; }
	void SetReadOnly(bool read_only) { this->read_only = read_only; }

	bool IsRemovable() const { return removable; }
	void SetRemovable(bool removable) { this->removable = removable; }
	bool IsRemoved() const { return removed; }
	void SetRemoved(bool removed) { this->removed = removed; }

	bool IsLockable() const { return lockable; }
	void SetLockable(bool lockable) { this->lockable = lockable; }
	bool IsLocked() const { return locked; }
	void SetLocked(bool locked) { this->locked = locked; }

	unsigned int GetId() const { return id; }
	void SetId(unsigned int id) { this->id = id; }
	unsigned int GetLun() const { return lun; }
	void SetLun(unsigned int lun) { this->lun = lun; }

	void SetVendor(const string&);
	void SetProduct(const string&, bool = true);
	void SetRevision(const string&);
	const string GetPaddedName() const;

	int GetStatusCode() const { return status_code; }
	void SetStatusCode(int status_code);

	virtual bool Eject(bool);

	bool IsSASI() const { return type == "SAHD"; }
	bool IsSCSI() const { return type == "SCHD" || type == "SCRM"; }
	bool IsCdRom() const { return type == "SCCD"; }
	bool IsMo() const { return type == "SCMO"; }
	bool IsBridge() const { return type == "SCBR"; }
	bool IsDaynaPort() const { return type == "SCDP"; }
};
