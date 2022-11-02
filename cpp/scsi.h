//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

#include <unordered_map>

using namespace std;

namespace scsi_defs {
	enum class scsi_level : int {
		SCSI_1_CCS = 1,
		SCSI_2 = 2,
		SPC = 3,
		SPC_2 = 4,
		SPC_3 = 5,
		SPC_4 = 6,
		SPC_5 = 7,
		SPC_6 = 8
	};

	enum class device_type : int {
		DIRECT_ACCESS = 0,
		PRINTER = 2,
		PROCESSOR = 3,
		CD_ROM = 5,
		OPTICAL_MEMORY = 7,
		COMMUNICATIONS = 9
	};

	enum class scsi_command : int {
		eCmdTestUnitReady = 0x00,
		eCmdRezero =  0x01,
		eCmdRequestSense = 0x03,
		eCmdFormatUnit = 0x04,
		eCmdReassignBlocks = 0x07,
		eCmdRead6 = 0x08,
		// Bridge specific command
		eCmdGetMessage10 = 0x08,
		// DaynaPort specific command
		eCmdRetrieveStats = 0x09,
		eCmdWrite6 = 0x0A,
		// Bridge specific ommand
		eCmdSendMessage10 = 0x0A,
		eCmdPrint = 0x0A,
		eCmdSeek6 = 0x0B,
		// DaynaPort specific command
		eCmdSetIfaceMode = 0x0C,
		// DaynaPort specific command
		eCmdSetMcastAddr  = 0x0D,
		// DaynaPort specific command
		eCmdEnableInterface = 0x0E,
		eCmdSynchronizeBuffer = 0x10,
		eCmdInquiry = 0x12,
		eCmdModeSelect6 = 0x15,
		eCmdReserve6 = 0x16,
		eCmdRelease6 = 0x17,
		eCmdModeSense6 = 0x1A,
		eCmdStartStop = 0x1B,
		eCmdStopPrint = 0x1B,
		eCmdSendDiagnostic = 0x1D,
		eCmdPreventAllowMediumRemoval = 0x1E,
		eCmdReadCapacity10 = 0x25,
		eCmdRead10 = 0x28,
		eCmdWrite10 = 0x2A,
		eCmdSeek10 = 0x2B,
		eCmdVerify10 = 0x2F,
		eCmdSynchronizeCache10 = 0x35,
		eCmdReadDefectData10 = 0x37,
		eCmdReadLong10 = 0x3E,
		eCmdWriteLong10 = 0x3F,
		eCmdReadToc = 0x43,
		eCmdGetEventStatusNotification = 0x4A,
		eCmdModeSelect10 = 0x55,
		eCmdModeSense10 = 0x5A,
		eCmdRead16 = 0x88,
		eCmdWrite16 = 0x8A,
		eCmdVerify16 = 0x8F,
		eCmdSynchronizeCache16 = 0x91,
		eCmdReadCapacity16_ReadLong16 = 0x9E,
		eCmdWriteLong16 = 0x9F,
		eCmdReportLuns = 0xA0
	};

	enum class status : int {
		GOOD = 0x00,
		CHECK_CONDITION = 0x02,
		RESERVATION_CONFLICT = 0x18
	};

	enum class sense_key : int {
		NO_SENSE = 0x00,
		NOT_READY = 0x02,
		MEDIUM_ERROR = 0x03,
		ILLEGAL_REQUEST = 0x05,
		UNIT_ATTENTION = 0x06,
		DATA_PROTECT = 0x07,
		ABORTED_COMMAND = 0x0b
	};

	enum class asc : int {
		NO_ADDITIONAL_SENSE_INFORMATION = 0x00,
		WRITE_FAULT = 0x03,
		READ_FAULT = 0x11,
		INVALID_COMMAND_OPERATION_CODE = 0x20,
		LBA_OUT_OF_RANGE = 0x21,
		INVALID_FIELD_IN_CDB = 0x24,
		INVALID_LUN = 0x25,
		INVALID_FIELD_IN_PARAMETER_LIST = 0x26,
		WRITE_PROTECTED = 0x27,
		NOT_READY_TO_READY_CHANGE = 0x28,
		POWER_ON_OR_RESET = 0x29,
		MEDIUM_NOT_PRESENT = 0x3a,
		LOAD_OR_EJECT_FAILED = 0x53
	};

	static const unordered_map<scsi_command, const char *> command_names = {
			{ scsi_command::eCmdTestUnitReady, "TestUnitReady" },
			{ scsi_command::eCmdRezero, "Rezero" },
			{ scsi_command::eCmdRequestSense, "RequestSense" },
			{ scsi_command::eCmdFormatUnit, "FormatUnit" },
			{ scsi_command::eCmdReassignBlocks, "ReassignBlocks" },
			{ scsi_command::eCmdRead6, "Read6/GetMessage10" },
			{ scsi_command::eCmdRetrieveStats, "RetrieveStats" },
			{ scsi_command::eCmdWrite6, "Write6/Print/SendMessage10" },
			{ scsi_command::eCmdSeek6, "Seek6" },
			{ scsi_command::eCmdSetIfaceMode, "SetIfaceMode" },
			{ scsi_command::eCmdSetMcastAddr, "SetMcastAddr" },
			{ scsi_command::eCmdEnableInterface, "EnableInterface" },
			{ scsi_command::eCmdSynchronizeBuffer, "SynchronizeBuffer" },
			{ scsi_command::eCmdInquiry, "Inquiry" },
			{ scsi_command::eCmdModeSelect6, "ModeSelect6" },
			{ scsi_command::eCmdReserve6, "Reserve6" },
			{ scsi_command::eCmdRelease6, "Release6" },
			{ scsi_command::eCmdModeSense6, "ModeSense6" },
			{ scsi_command::eCmdStartStop, "StartStop" },
			{ scsi_command::eCmdStopPrint, "StopPrint" },
			{ scsi_command::eCmdSendDiagnostic, "SendDiagnostic" },
			{ scsi_command::eCmdPreventAllowMediumRemoval, "PreventAllowMediumRemoval" },
			{ scsi_command::eCmdReadCapacity10, "ReadCapacity10" },
			{ scsi_command::eCmdRead10, "Read10" },
			{ scsi_command::eCmdWrite10, "Write10" },
			{ scsi_command::eCmdSeek10, "Seek10" },
			{ scsi_command::eCmdVerify10, "Verify10" },
			{ scsi_command::eCmdSynchronizeCache10, "SynchronizeCache10" },
			{ scsi_command::eCmdReadDefectData10, "ReadDefectData10" },
			{ scsi_command::eCmdReadLong10, "ReadLong10" },
			{ scsi_command::eCmdWriteLong10, "WriteLong10" },
			{ scsi_command::eCmdReadToc, "ReadToc" },
			{ scsi_command::eCmdGetEventStatusNotification, "GetEventStatusNotification" },
			{ scsi_command::eCmdModeSelect10, "ModeSelect10" },
			{ scsi_command::eCmdModeSense10, "ModeSense10" },
			{ scsi_command::eCmdRead16, "Read16" },
			{ scsi_command::eCmdWrite16, "Write16" },
			{ scsi_command::eCmdVerify16, "Verify16" },
			{ scsi_command::eCmdSynchronizeCache16, "SynchronizeCache16" },
			{ scsi_command::eCmdReadCapacity16_ReadLong16, "ReadCapacity16/ReadLong16" },
			{ scsi_command::eCmdWriteLong16, "WriteLong16" },
			{ scsi_command::eCmdReportLuns, "ReportLuns" }
	};
};
