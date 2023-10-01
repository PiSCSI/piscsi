//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

//---------------------------------------------------------------------------
//
//	Various Operation Settings
//
//---------------------------------------------------------------------------
#define USE_SEL_EVENT_ENABLE			// Check SEL signal by event
// This avoids an indefinite loop with warnings if there is no PiSCSI hardware
// and thus helps with running scsictl and unit test on x86 hardware.
#if defined(__x86_64__) || defined(__X86__) || defined(__aarch64__) || !defined(__linux__)
#undef USE_SEL_EVENT_ENABLE
#endif
