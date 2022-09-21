//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
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
	explicit illegal_argument_exception(const string& msg) : msg(msg) {}
	~illegal_argument_exception() override = default;

	const string& get_msg() const { return msg; }
};

class io_exception : public exception {
private:
	string msg;

public:
	explicit io_exception(const string& msg) : msg(msg) {}
	~io_exception() override = default;

	const string& get_msg() const { return msg; }
};

class file_not_found_exception : public io_exception {
	using io_exception::io_exception;
};

class scsi_error_exception final : public exception {
private:
	scsi_defs::sense_key sense_key;
	scsi_defs::asc asc;
	scsi_defs::status status;

public:
	scsi_error_exception(scsi_defs::sense_key sense_key = scsi_defs::sense_key::ABORTED_COMMAND,
			scsi_defs::asc asc = scsi_defs::asc::NO_ADDITIONAL_SENSE_INFORMATION,
			scsi_defs::status status = scsi_defs::status::CHECK_CONDITION)
	: sense_key(sense_key), asc(asc), status(status) {}
	~scsi_error_exception() final = default;

	scsi_defs::sense_key get_sense_key() const { return sense_key; }
	scsi_defs::asc get_asc() const { return asc; }
	scsi_defs::status get_status() const { return status; }
};
