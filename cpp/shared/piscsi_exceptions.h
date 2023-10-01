//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "scsi.h"
#include <stdexcept>

using namespace std;

class parser_exception : public runtime_error
{
	using runtime_error::runtime_error;
};

class io_exception : public runtime_error
{
	using runtime_error::runtime_error;
};

class file_not_found_exception : public io_exception
{
	using io_exception::io_exception;
};

class scsi_exception : public exception
{
	scsi_defs::sense_key sense_key;
	scsi_defs::asc asc;

public:

	scsi_exception(scsi_defs::sense_key sense_key, scsi_defs::asc asc = scsi_defs::asc::no_additional_sense_information)
		: sense_key(sense_key), asc(asc) {}
	~scsi_exception() override = default;

	scsi_defs::sense_key get_sense_key() const { return sense_key; }
	scsi_defs::asc get_asc() const { return asc; }
};
