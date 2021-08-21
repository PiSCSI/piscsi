//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Various exceptions
//
//---------------------------------------------------------------------------

#pragma once

#include <exception>
#include <string>

using namespace std;

class illegal_argument_exception final : public exception {
private:
	string msg;

public:
	illegal_argument_exception(const string& _msg) : msg(_msg) {}
	illegal_argument_exception() {};

	const string& getmsg() const {
		return msg;
	}
};

class lun_exception final : public exception {
private:
	int lun;

public:
	lun_exception(int _lun) : lun(_lun) {}
	~lun_exception() {}

	int getlun() const {
		return lun;
	}
};

class io_exception : public exception {
private:
	string msg;

public:
	io_exception(const string& _msg) : msg(_msg) {}
	~io_exception() {}

	const string& getmsg() const {
		return msg;
	}
};
