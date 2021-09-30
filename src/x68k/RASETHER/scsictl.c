//---------------------------------------------------------------------------
//
//  SCSI Target Emulator RaSCSI (*^..^*)
//  for Raspberry Pi
//
//  Powered by XM6 TypeG Technology.
//  Copyright (C) 2016-2017 GIMONS
//	[ RaSCSI Ethernet SCSI Control Department ]
//
//  Based on
//    Neptune-X board driver for  Human-68k(ESP-X)   version 0.03
//    Programed 1996-7 by Shi-MAD.
//    Special thanks to  Niggle, FIRST, yamapu ...
//
//---------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <sys/dos.h>
#include <sys/scsi.h>
#include <iocslib.h>
#include "main.h"
#include "scsictl.h"

typedef struct
{
	char DeviceType;
	char RMB;
	char ANSI_Ver;
	char RDF;
	char AddLen;
	char RESV0;
	char RESV1;
	char OptFunc;
	char VendorID[8];
	char ProductID[16];
	char FirmRev[4];
} INQUIRY_T;

typedef struct
{
	INQUIRY_T info;
	char options[8];
} INQUIRYOPT_T;

#define MFP_AEB		0xe88003
#define MFP_IERB	0xe88009
#define MFP_IMRB	0xe88015

// Subroutine in asmsub.s
extern void DI();
extern void EI();

volatile short* iocsexec = (short*)0xa0e;	// Work when executin IOCS
struct trans_counter trans_counter;			// Transfer counter
unsigned char rx_buff[2048];				// Receive buffer
int imr;									// Interrupt permission flag
int scsistop;								// SCSI stopped flag
int intr_type;								// Interrupt type (0:V-DISP 1:TimerA)
int poll_interval;							// Polling interval (configure)
int poll_current;							// Polling interval (current)
int idle;									// Idle counter

#define POLLING_SLEEP	255	// 4-5s

/************************************************
 *   Execute command to get MAC address         *
 ************************************************/
int SCSI_GETMACADDR(unsigned char *mac)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	cmd[8] = 6;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	if (S_DATAIN(6, mac) != 0) {
#else
	if (S_DATAIN_P(6, mac) != 0) {
#endif
		return 0;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	return 1;
}

/************************************************
 *   Execute command to configure MAC address   *
 ************************************************/
int SCSI_SETMACADDR(const unsigned char *mac)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x2a;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 0;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	cmd[8] = 6;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	S_DATAOUT(6, mac);
#else
	S_DATAOUT_P(6, mac);
#endif

	S_STSIN(&sts);
	S_MSGIN(&msg);
	
	return 1;
}

/************************************************
 *   Execute command to get received packet size*
 ************************************************/
int SCSI_GETPACKETLEN(int *len)
{
	unsigned char cmd[10];
	unsigned char buf[2];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = 0;
	cmd[7] = 0;
	cmd[8] = 2;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	if (S_DATAIN(2, buf) != 0) {
#else
	if (S_DATAIN_P(2, buf) != 0) {
#endif
		return 0;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	*len = (int)(buf[0] << 8) + (int)(buf[1]);

	return 1;
}

/************************************************
 *   Execute receive packet command             *
 ************************************************/
int SCSI_GETPACKETBUF(unsigned char *buf, int len)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x28;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = (unsigned char)(len >> 16);
	cmd[7] = (unsigned char)(len >> 8);
	cmd[8] = (unsigned char)len;
	cmd[9] = 1;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	if (S_DATAIN(len, buf) != 0) {
#else
	if (S_DATAIN_P(len, buf) != 0) {
#endif
		return 0;
	}

	S_STSIN(&sts);
	S_MSGIN(&msg);

	return 1;
}

/************************************************
 *   Execute packet send command                *
 ************************************************/
int SCSI_SENDPACKET(const unsigned char *buf, int len)
{
	unsigned char cmd[10];
	unsigned char sts;
	unsigned char msg;
	
	if (S_SELECT(scsiid) != 0) {
		return 0;
	}

	cmd[0] = 0x2a;
	cmd[1] = 0;
	cmd[2] = 1;
	cmd[3] = 1;
	cmd[4] = 0;
	cmd[5] = 0;
	cmd[6] = (unsigned char)(len >> 16);
	cmd[7] = (unsigned char)(len >> 8);
	cmd[8] = (unsigned char)len;
	cmd[9] = 0;
	if (S_CMDOUT(10, cmd) != 0) {
		return 0;
	}

#ifdef USE_DMA
	S_DATAOUT(len, buf);
#else
	S_DATAOUT_P(len, buf);
#endif
	S_STSIN(&sts);
	S_MSGIN(&msg);
	
	return 1;
}

/************************************************
 *   Get MAC address                            *
 ************************************************/
int GetMacAddr(struct eaddr* buf)
{
	if (SCSI_GETMACADDR(buf->eaddr) != 0) {
		return 0;
	} else {
		return -1;
	}
}

/************************************************
 *   Set MAC address                            *
 ************************************************/
int SetMacAddr(const struct eaddr* data)
{
	if (SCSI_SETMACADDR(data->eaddr) != 0) {
		return 0;
	} else {
		return -1;
	}
}

/************************************************
 *  Search RaSCSI                               *
 ************************************************/
int SearchRaSCSI()
{
	int i;
	INQUIRYOPT_T inq;

	for (i = 0; i <= 7; i++) {
		// Search for BRIDGE device
		if (S_INQUIRY(sizeof(INQUIRY_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}

		if (memcmp(&(inq.info.ProductID), "RASCSI BRIDGE", 13) != 0) {
			continue;
		}

		// Get TAP initialization status
		if (S_INQUIRY(sizeof(INQUIRYOPT_T) , i, (struct INQUIRY*)&inq) < 0) {
			continue;
		}
		
		if (inq.options[1] != '1') {
			continue;
		}

		// Configure SCSI ID
		scsiid = i;
		return 0;
	}

	return -1;
}

/************************************************
 *  Init RaSCSI method                          *
 ************************************************/
int InitRaSCSI(void)
{
#ifdef MULTICAST
	unsigned char multicast_table[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
#endif

	imr = 0;
	scsistop = 0;
	poll_current = -1;
	idle = 0;
	
	return 0;
}

/************************************************
 *  RaSCSI interrupt handler function (polling) *
 ************************************************/
void interrupt IntProcess(void)
{
	unsigned char phase;
	unsigned int len;
	int type;
	int_handler func;
	int i;

	// V-DISP GPIP interrupt idle count control
	if (intr_type == 0) {
		// Increment idle
		idle++;
		
		// Skip if not yet next scheduled processing
		if (idle < poll_current) {
			return;
		}

		// Clear idle counter
		idle = 0;
	}
	
	// Start interrupt

	// Only when interrupt is permitted
	if (imr == 0) {
		return;
	}

	// Skip if executing IOCS
	if (*iocsexec != -1) {
		return;
	}

	// Interrupt forbidden if receiving data
	DI ();

	// Only in bus free phase
	phase = S_PHASE();
	if (phase != 0) {
		// Exit
		goto ei_exit;
	}	

	// Receive data
	if (SCSI_GETPACKETLEN(&len) == 0) {
		// RaSCSI is stopped
		scsistop = 1;

		// Reset polling interval (sleep)
		UpdateIntProcess(POLLING_SLEEP);

		// Exit
		goto ei_exit;
	}
	
	// RaSCSI is stopped
	if (scsistop) {
		scsistop = 0;

		// Reset polling interval (hurry)
		UpdateIntProcess(poll_interval);
	}
	
	// Packets did not arrive
	if (len == 0) {
		// Exit
		goto ei_exit;
	}

	// Tranfer packets to receive buffer memory
	if (SCSI_GETPACKETBUF(rx_buff, len) == 0) {
		// Fail
		goto ei_exit;
	}

	// Interrupt permitted
	EI ();
	
	// Split data by packet type
	type = rx_buff[12] * 256 + rx_buff[13];
	i = 0;
	while ((func = SearchList2(type, i))) {
		i++;
		func(len, (void*)&rx_buff, ID_EN0);
	}
	trans_counter.recv_byte += len;
	return;

ei_exit:
	// Interrupt permitted
	EI ();
}

/************************************************
 *  RaSCSI Send Packets Function (from ne.s)    *
 ************************************************/
int SendPacket(int len, const unsigned char* data)
{
	if (len < 1) {
		return 0;
	}
	
	if (len > 1514)	{		// 6 + 6 + 2 + 1500
		return -1;			// Error
	}
	
	// If RaSCSI seems to be stopped, throw an error
	if (scsistop) {
		return -1;
	}

	// Interrupt is not permitted during sending
	DI ();

	// Send processing and raise send flag
	if (SCSI_SENDPACKET(data, len) == 0) {
		// Interrupt permitted
		EI ();
		return -1;
	}

	// Interrupt permitted
	EI ();

	// Finished requesting send
	trans_counter.send_byte += len;
	
	return 0;
}

/************************************************
 *  RaSCSI Interrupt Permission Setting Function*
 ************************************************/
int SetPacketReception(int i)
{
	imr = i;
	return 0;
}

/************************************************
 * RaSCSI Interrupt Processing Registration     *
 ************************************************/
void RegisterIntProcess(int n)
{
	volatile unsigned char *p;

	// Update polling interval (current) and clear idle counter
	poll_current = n;
	idle = 0;
	
	if (intr_type == 0) {
		// Overwrite V-DISP GPIP interrupt vectors
		// and enable interrupt
		B_INTVCS(0x46, (int)IntProcess);
		p = (unsigned char *)MFP_AEB;
		*p = *p | 0x10;
		p = (unsigned char *)MFP_IERB;
		*p = *p | 0x40;
		p = (unsigned char *)MFP_IMRB;
		*p = *p | 0x40;
	} else if (intr_type == 1) {
		// Set TimerA counter mode
		VDISPST(NULL, 0, 0);
		VDISPST(IntProcess, 0, poll_current);
	}
}

/************************************************
 * RaSCSI Interrupt Processing Update           *
 ************************************************/
void UpdateIntProcess(int n)
{
	// Do not update if polling interval (current) is the same
	if (n == poll_current) {
		return;
	}
	
	// Update polling interval (current) and clear idle counter
	poll_current = n;
	idle = 0;
	
	if (intr_type == 1) {
		// TimerA requires re-registering
		VDISPST(NULL, 0, 0);
		VDISPST(IntProcess, 0, poll_current);
	}
}

// EOF
