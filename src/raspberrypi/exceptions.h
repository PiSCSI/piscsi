//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi.h"
#include <exception>
#include <string>

using namespace std;

class illegal_argument_exception final : public exception {
private:
	string msg;

public:
	illegal_argument_exception(const string& _msg) : msg(_msg) {}
	illegal_argument_exception() {};

	const string& get_msg() const { return msg; }
};

class io_exception : public exception {
private:
	string msg;

public:
	io_exception(const string& _msg) : msg(_msg) {}
	virtual ~io_exception() {}

	const string& get_msg() const { return msg; }
};

class file_not_found_exception : public io_exception {
public:
	file_not_found_exception(const string& msg) : io_exception(msg) {}
	~file_not_found_exception() {}
};

// Results in an SCSI error to be reported. It may currently only be raised within the command dispatcher call hierarchy.
class scsi_dispatch_error_exception : public exception {
private:
	scsi_defs::sense_key sense_key;
	scsi_defs::asc asc;
	scsi_defs::status status;

public:
	scsi_dispatch_error_exception(scsi_defs::sense_key sense_key = scsi_defs::sense_key::ABORTED_COMMAND,
			scsi_defs::asc asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status status = scsi_defs::status::CHECK_CONDITION) {
		this->sense_key = sense_key;
		this->asc = asc;
		this->status = status;
	}
	~scsi_dispatch_error_exception() {};

	scsi_defs::sense_key get_sense_key() const { return sense_key; }
	scsi_defs::asc get_asc() const { return asc; }
	scsi_defs::status get_status() const { return status; }
};
