//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI for Raspberry Pi
//  Loopback tester utility
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include "scsiloop_cout.h"
#include <iostream>

using namespace std;

void ScsiLoop_Cout::StartTest(const string &test_name)
{
    cout << CYAN << "Testing " << test_name << ":" << WHITE;
}
void ScsiLoop_Cout::PrintUpdate()
{
    cout << ".";
}

void ScsiLoop_Cout::FinishTest(const string &test_name, int failures)
{
    if (failures == 0) {
        cout << GREEN << "OK!" << WHITE << endl;
    } else {
        cout << RED << test_name << " FAILED! - " << failures << " errors!" << WHITE << endl;
    }
}

void ScsiLoop_Cout::PrintErrors(vector<string> &test_errors)
{
    if (test_errors.size() > 0) {
        for (auto err_string : test_errors) {
            cout << RED << err_string << endl;
        }
    }
}