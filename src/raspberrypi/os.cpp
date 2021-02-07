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
#include "log.h"
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


//---------------------------------------------------------------------------
//
//	Run system command and save the output to a string
//
//---------------------------------------------------------------------------
int run_system_cmd_with_output(const char* command, char* output_str, size_t max_size)
{
	FILE *fp;
	char str_buff[1024];

	fp = popen(command,"r");
	if(fp == NULL)
	{
		LOGWARN("%s Unable to run command %s", __PRETTY_FUNCTION__, command);
		return EXIT_FAILURE;
	}

	while((fgets(str_buff, sizeof(str_buff), fp) != NULL) && (strlen(output_str) < max_size))
	{
		strncat(output_str, str_buff, max_size);
	}

	pclose(fp);
	return EXIT_SUCCESS;
}