//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//	Powered by XM6 TypeG Technology.
//	Copyright (C) 2016-2020 GIMONS
//
//	[ Common Definitions ]
//
//---------------------------------------------------------------------------

#pragma once

//---------------------------------------------------------------------------
//
//	Various Operation Settings
//
//---------------------------------------------------------------------------
#define USE_SEL_EVENT_ENABLE			// Check SEL signal by event
// This avoids an indefinite loop with warnings if there is no RaSCSI hardware
// and thus helps with running certain tests on X86 hardware.
#if defined(__x86_64) || defined(__X86)
#undef USE_SEL_EVENT_ENABLE
#endif
