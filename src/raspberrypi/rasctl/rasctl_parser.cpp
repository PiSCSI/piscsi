//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rasctl_parser.h"

PbOperation RasctlParser::ParseOperation(const string& operation) const
{
	switch (tolower(operation[0])) {
		case 'a':
			return ATTACH;

		case 'd':
			return DETACH;

		case 'i':
			return INSERT;

		case 'e':
			return EJECT;

		case 'p':
			return PROTECT;

		case 'u':
			return UNPROTECT;

		case 's':
			return DEVICES_INFO;

		default:
			return NO_OPERATION;
	}
}

PbDeviceType RasctlParser::ParseType(const char *type) const
{
	string t = type;
	transform(t.begin(), t.end(), t.begin(), ::toupper);

	if (PbDeviceType parsed_type; PbDeviceType_Parse(t, &parsed_type)) {
		return parsed_type;
	}

	// Parse convenience device types (shortcuts)
	switch (tolower(type[0])) {
	case 'c':
		return SCCD;

	case 'b':
		return SCBR;

	case 'd':
		return SCDP;

	case 'h':
		return SCHD;

	case 'm':
		return SCMO;

	case 'r':
		return SCRM;

	case 'l':
		return SCLP;

	case 's':
		return SCHS;

	default:
		return UNDEFINED;
	}
}
