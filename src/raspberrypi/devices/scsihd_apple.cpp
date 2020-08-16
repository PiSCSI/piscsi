//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//  Copyright (C) akuker
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ SCSI Hard Disk for Apple Macintosh ]
//
//---------------------------------------------------------------------------

#include "scsihd_apple.h"

//===========================================================================
//
//	SCSI hard disk (Macintosh Apple genuine)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIHD_APPLE::SCSIHD_APPLE() : SCSIHD()
{
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_APPLE::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	int size;
	char vendor[32];
	char product[32];

	// Call the base class
	size = SCSIHD::Inquiry(cdb, buf, major, minor);

	// End if there is an error in the base class
	if (size == 0) {
		return 0;
	}

	// Vendor name
	sprintf(vendor, " SEAGATE");
	memcpy(&buf[8], vendor, strlen(vendor));

	// Product name
	sprintf(product, "          ST225N");
	memcpy(&buf[16], product, strlen(product));

	return size;
}

//---------------------------------------------------------------------------
//
//	Add Vendor special page
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD_APPLE::AddVendor(int page, BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Page code 48
	if ((page != 0x30) && (page != 0x3f)) {
		return 0;
	}

	// Set the message length
	buf[0] = 0x30;
	buf[1] = 0x1c;

	// No changeable area
	if (change) {
		return 30;
	}

	// APPLE COMPUTER, INC.
	memcpy(&buf[0xa], "APPLE COMPUTER, INC.", 20);

	return 30;
}
