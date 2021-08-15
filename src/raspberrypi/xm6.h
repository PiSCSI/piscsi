//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	[ Common Definition ]
//
//---------------------------------------------------------------------------

#if !defined(xm6_h)
#define xm6_h

//---------------------------------------------------------------------------
//
//	Various Operation Settings
//
//---------------------------------------------------------------------------
#define USE_SEL_EVENT_ENABLE			// Check SEL signal by event
#define REMOVE_FIXED_SASIHD_SIZE		// remove the size limitation of SASIHD
#define USE_MZ1F23_1024_SUPPORT			// MZ-1F23 (SASI 20M/sector size 1024)
// This avoids an indefinite loop with warnings if there is no RaSCSI hardware
// and thus helps with running certain tests on X86 hardware.
#if defined(__x86_64__) || defined(__X86__)
#undef USE_SEL_EVENT_ENABLE
#endif

//---------------------------------------------------------------------------
//
//	Class Declaration
//
//---------------------------------------------------------------------------
class Fileio;
										// File I/O
class Disk;
										// SASI/SCSI Disk
class Filepath;
										// File Path

#endif	// xm6_h
