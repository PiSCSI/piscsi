class lunexception : public std::exception {
private:
        DWORD lun;

public:
        lunexception(DWORD lun) {
                this->lun = lun;
        }

        ~lunexception() { }

        DWORD getlun() {
            return lun;
        }
};
