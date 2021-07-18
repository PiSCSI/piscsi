//---------------------------------------------------------------------------
//
//      SCSI Target Emulator RaSCSI (*^..^*)
//      for Raspberry Pi
//
//      Powered by XM6 TypeG Technology.
//      Copyright (C) 2016-2020 GIMONS
//      [ Exceptions ]
//
//---------------------------------------------------------------------------

#if !defined(exceptions_h)
#define exceptions_h

#include <exception>

class lunexception : public std::exception {
private:
        int lun;

public:
        lunexception(int _lun) : lun(_lun) { }

        ~lunexception() { }

        int getlun() const {
            return lun;
        }
};

class ioexception : public std::exception {
private:
	const char *msg;

public:
	ioexception(const char *_msg) : msg(_msg) { }

	~ioexception() { }

	const char *getmsg() const {
		return msg;
	}
};

#endif
