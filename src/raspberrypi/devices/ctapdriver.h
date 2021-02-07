//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//  Copyright (C) akuker
//
//	Imported NetBSD support and some optimisation patch by Rin Okuyama.
//
//	[ TAP Driver ]
//
//---------------------------------------------------------------------------

#if !defined(ctapdriver_h)
#define ctapdriver_h

#ifndef ETH_FRAME_LEN
#define ETH_FRAME_LEN 1514
#endif

//===========================================================================
//
//	Linux Tap Driver
//
//===========================================================================
class CTapDriver
{
public:
	// Basic Functionality
	CTapDriver();
										// Constructor
	BOOL FASTCALL Init();
										// Initilization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL GetMacAddr(BYTE *mac);
										// Get Mac Address
	int FASTCALL Rx(BYTE *buf);
										// Receive
	int FASTCALL Tx(const BYTE *buf, int len);
										// Send
	BOOL FASTCALL PendingPackets();
										// Check if there are IP packets available
	BOOL FASTCALL Enable();
										// Enable the ras0 interface
	BOOL FASTCALL Disable();
										// Disable the ras0 interface
	BOOL FASTCALL Flush();
										// Purge all of the packets that are waiting to be processed

private:
	BYTE m_MacAddr[6];
										// MAC Address
	BOOL m_bTxValid;
										// Send Valid Flag
	int m_hTAP;
										// File handle
	BYTE m_garbage_buffer[ETH_FRAME_LEN];
};

#endif	// ctapdriver_h
