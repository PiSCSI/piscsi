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

class ScsiLoop_Timer
{
  public:
    static int RunTimerTest(vector<string> &error_list);
};