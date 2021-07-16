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

class lunexception : public std::exception {
private:
        DWORD lun;

public:
        lunexception(DWORD lun) {
                this->lun = lun;
        }

        ~lunexception() { }

        DWORD getlun() const {
            return lun;
        }
};

#endif
