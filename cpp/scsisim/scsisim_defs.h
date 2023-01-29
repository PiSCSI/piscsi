//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI for Raspberry Pi
//
//	Copyright (C) 2023 akuker
//
//	[ SCSI Bus Emulator Common Definitions]
//
//---------------------------------------------------------------------------

#pragma once

#include <string>

using namespace std;

const string SHARED_MEM_MUTEX_NAME = "/piscsi-sem-mutex";
const string SHARED_MEM_NAME       = "/piscsi-shared-mem";
