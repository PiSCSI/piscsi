//---------------------------------------------------------------------------
//
//      SCSI Target Emulator RaSCSI (*^..^*)
//      for Raspberry Pi
//
//      Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <exception>
#include <string>

using namespace std;

class illegalargumentexception final : public exception {
private:
	string msg;

public:
	illegalargumentexception(const string& _msg) : msg(_msg) {}
	illegalargumentexception() {};

	const string& getmsg() const {
		return msg;
	}
};

class lunexception final : public exception {
private:
	int lun;

public:
	lunexception(int _lun) : lun(_lun) {}
	~lunexception() {}

	int getlun() const {
		return lun;
	}
};

class ioexception : public exception {
private:
	string msg;

public:
	ioexception(const string& _msg) : msg(_msg) {}
	~ioexception() {}

	const string& getmsg() const {
		return msg;
	}
};
