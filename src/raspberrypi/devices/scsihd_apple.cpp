//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License. 
//	See LICENSE file in the project root folder.
//
//	[ SCSI Hard Disk for Apple Macintosh ]
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
	SetVendor(" SEAGATE");
	SetProduct("          ST225N");
}

//---------------------------------------------------------------------------
//
//	Add Vendor special page
//
//---------------------------------------------------------------------------
int SCSIHD_APPLE::AddVendor(int page, BOOL change, BYTE *buf)
{
	ASSERT(buf);

	// Page code 48 or 63
	if (page != 0x30 && page != 0x3f) {
		return 0;
	}

	// Set the message length
	buf[0] = 0x30;
	buf[1] = 0x1c;

	// No changeable area
	if (!change) {
		memcpy(&buf[0xa], "APPLE COMPUTER, INC.", 20);
	}

	return 30;
}
