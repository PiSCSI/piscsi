//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
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
	int FASTCALL Tx(BYTE *buf, int len);
										// Send

private:
	BYTE m_MacAddr[6];
										// MAC Address
	BOOL m_bTxValid;
										// Send Valid Flag
	int m_hTAP;
										// File handle
};

#endif	// ctapdriver_h
