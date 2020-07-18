//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//	[ ログ ]
//
//---------------------------------------------------------------------------

#if !defined(log_h)
#define log_h


#define LOGINFO(...)  \
 do{char buf[256]; snprintf(buf, 256,__VA_ARGS__);  spdlog::info(buf);}while(0)

//===========================================================================
//
//	ログ
//
//===========================================================================
class Log
{
public:
	enum loglevel {
		Detail,							// 詳細レベル
		Normal,							// 通常レベル
		Warning,						// 警告レベル
		Debug							// デバッグレベル
	};
};

#endif	// log_h
