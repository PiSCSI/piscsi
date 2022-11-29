
#include "scsiloop_cout.h"
#include <iostream>

using namespace std;

void ScsiLoop_Cout::StartTest(const string &test_name)
{
    cout << CYAN << "Testing " << test_name << ":" << WHITE;
    // printf("%sTesting %s:%s", CYAN.c_str(), test_name.c_str(), WHITE.c_str());
}
void ScsiLoop_Cout::PrintUpdate()
{
    cout << ".";
    // printf("%s.", WHITE.c_str());
}

void ScsiLoop_Cout::FinishTest(const string &test_name, int failures)
{
    if (failures == 0) {
        cout << GREEN << "OK!" << WHITE << endl;
        // printf("%sOK!\n%s", GREEN.c_str(), WHITE.c_str());
    } else {
        cout << RED << test_name << " FAILED! - " << failures << " errors!" << WHITE << endl;
        // printf("%s%s FAILED - %d errors!\n%s", RED.c_str(), test_name, failures, WHITE.c_str());
    }
}

void ScsiLoop_Cout::PrintErrors(vector<string> &test_errors)
{
    if (test_errors.size() > 0) {
        for (auto err_string : test_errors) {
            cout << RED << err_string << endl;
            // printf("%s%s\n", RED.c_str(), err_string.c_str());
        }
    }
}