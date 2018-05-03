//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technorogy.
//  Copyright (C) 2016-2017 GIMONS
//	[ RaSCSI イーサーネット SCSI制御部 ]
//
//  Based on
//    Neptune-X board driver for  Human-68k(ESP-X)   version 0.03
//    Programed 1996-7 by Shi-MAD.
//    Special thanks to  Niggle, FIRST, yamapu ...
//
//---------------------------------------------------------------------------

#ifndef scsictl_h
#define scsictl_h

// グローバル変数
extern int intr_type;
extern int poll_interval;

// 送受信カウンタ
struct trans_counter {
	unsigned int send_byte;
	unsigned int recv_byte;
};
extern struct trans_counter trans_counter;

extern int SearchRaSCSI();
extern int InitRaSCSI(void);
extern int GetMacAddr(struct eaddr* buf);
extern int SetMacAddr(const struct eaddr* data);
extern int SetPacketReception(int i);
extern int SendPacket(int len, const unsigned char* data);
extern void RegisterIntProcess(int n);
extern void UpdateIntProcess(int n);

#endif // scsictl_h
