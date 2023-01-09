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

class io_exception : public exception {
private:
	string msg;

public:
	io_exception(const string& _msg) : msg(_msg) {}
	virtual ~io_exception() {}

	const string& getmsg() const {
		return msg;
	}
};

class file_not_found_exception : public io_exception {
public:
	file_not_found_exception(const string& msg) : io_exception(msg) {}
	~file_not_found_exception() {}
};
