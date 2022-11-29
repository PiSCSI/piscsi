//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded for Raspberry Pi
//  Loopback tester utility
//
//	Copyright (C) 2022 akuker
//
//---------------------------------------------------------------------------

#include <string>
#include <vector>

using namespace std;

class ScsiLoop_Cout
{
  public:
    static void StartTest(const string &test_name);
    static void PrintUpdate();
    static void FinishTest(const string &test_name, int failures);
    static void PrintErrors(vector<string> &test_errors);

  private:
    const static inline string RESET   = "\033[0m";
    const static inline string BLACK   = "\033[30m"; /* Black */
    const static inline string RED     = "\033[31m"; /* Red */
    const static inline string GREEN   = "\033[32m"; /* Green */
    const static inline string YELLOW  = "\033[33m"; /* Yellow */
    const static inline string BLUE    = "\033[34m"; /* Blue */
    const static inline string MAGENTA = "\033[35m"; /* Magenta */
    const static inline string CYAN    = "\033[36m"; /* Cyan */
    const static inline string WHITE   = "\033[37m"; /* White */
};