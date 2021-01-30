//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) 2020-2021 akuker
//
//	[ OS related definitions ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include <sys/wait.h>


//---------------------------------------------------------------------------
//
//	Run system command and wait for it to finish
//
//---------------------------------------------------------------------------
int run_system_cmd(const char* command)
{
	pid_t pid;
	int result;
	int status = 0;
	pid=fork();
	if(pid == 0){
		result = system(command);
		exit(result);
	}
	waitpid(pid, &status, 0);
	return status;
}