//---------------------------------------------------------------------------
//
// X68000 EMULATOR "XM6"
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <span>
#include <unordered_map>
#include <string>

using namespace std;

// Command Descriptor Block
using cdb_t = span<const int>;

namespace scsi_defs
{
enum class scsi_level {
    scsi_1_ccs = 1,
    scsi_2     = 2,
    spc        = 3,
    spc_2      = 4,
    spc_3      = 5,
    spc_4      = 6,
    spc_5      = 7,
    spc_6      = 8
};

// Phase definitions
enum class phase_t {
    busfree,
    arbitration,
    selection,
    reselection,
    command,
    datain,
    dataout,
    status,
    msgin,
    msgout,
    reserved
};

enum class device_type {
    direct_access  = 0,
    sad            = 1,
    printer        = 2,
    processor      = 3,
    cd_rom         = 5,
    optical_memory = 7,
    communications = 9
};

enum class scsi_command {
    eCmdTestUnitReady  = 0x00,
    eCmdRezero         = 0x01,
    eCmdRequestSense   = 0x03,
    eCmdFormatUnit     = 0x04,
    eCmdReadBlockLimits= 0x05,
    eCmdReassignBlocks = 0x07,
    eCmdRead6          = 0x08,
    // Bridge specific command
    eCmdGetMessage10 = 0x08,
    // DaynaPort specific command
    eCmdRetrieveStats = 0x09,
    eCmdWrite6        = 0x0A,
    // Bridge specific ommand
    eCmdSendMessage10 = 0x0A,
    eCmdPrint         = 0x0A,
    eCmdSeek6         = 0x0B,
    // DaynaPort specific command
    eCmdSetIfaceMode = 0x0C,
    // DaynaPort specific command
    eCmdSetMcastAddr = 0x0D,
    // DaynaPort specific command
    eCmdEnableInterface            = 0x0E,
    eCmdSynchronizeBuffer          = 0x10,
    eCmdWriteFilemarks             = 0x10,
    eCmdSpace                      = 0x11,
    eCmdInquiry                    = 0x12,
    eCmdVerify                     = 0x13,
    eCmdModeSelect6                = 0x15,
    eCmdReserve6                   = 0x16,
    eCmdRelease6                   = 0x17,
    eCmdErase                      = 0x19,
    eCmdModeSense6                 = 0x1A,
    eCmdStartStop                  = 0x1B,
    eCmdStopPrint                  = 0x1B,
    eCmdSendDiagnostic             = 0x1D,
    eCmdPreventAllowMediumRemoval  = 0x1E,
    eCmdReadCapacity10             = 0x25,
    eCmdRead10                     = 0x28,
    eCmdWrite10                    = 0x2A,
    eCmdSeek10                     = 0x2B,
    eCmdVerify10                   = 0x2F,
    eCmdReadPosition               = 0x34,
    eCmdSynchronizeCache10         = 0x35,
    eCmdReadDefectData10           = 0x37,
    eCmdReadLong10                 = 0x3E,
    eCmdWriteLong10                = 0x3F,
    eCmdReadToc                    = 0x43,
    eCmdGetEventStatusNotification = 0x4A,
    eCmdModeSelect10               = 0x55,
    eCmdModeSense10                = 0x5A,
    eCmdRead16                     = 0x88,
    eCmdWrite16                    = 0x8A,
    eCmdVerify16                   = 0x8F,
    eCmdSynchronizeCache16         = 0x91,
    eCmdReadCapacity16_ReadLong16  = 0x9E,
    eCmdWriteLong16                = 0x9F,
    eCmdReportLuns                 = 0xA0
};

enum class status {
	good 				 = 0x00,
	check_condition 	 = 0x02,
	reservation_conflict = 0x18
};

enum class sense_key {
    no_sense        = 0x00,
    not_ready       = 0x02,
    medium_error    = 0x03,
    illegal_request = 0x05,
    unit_attention  = 0x06,
    data_protect    = 0x07,
    blank_check     = 0x08,
    aborted_command = 0x0b
};

enum class asc {
    no_additional_sense_information = 0x00,
    write_fault                     = 0x03,
    read_fault                      = 0x11,
    parameter_list_length_error     = 0x1a,
    invalid_command_operation_code  = 0x20,
    lba_out_of_range                = 0x21,
    invalid_field_in_cdb            = 0x24,
    invalid_lun                     = 0x25,
    invalid_field_in_parameter_list = 0x26,
    write_protected                 = 0x27,
    not_ready_to_ready_change       = 0x28,
    power_on_or_reset               = 0x29,
    medium_not_present              = 0x3a,
    load_or_eject_failed            = 0x53
};

static const unordered_map<scsi_command, pair<int, string>> command_mapping = {
    {scsi_command::eCmdTestUnitReady, make_pair(6, "TestUnitReady")},
    {scsi_command::eCmdRezero, make_pair(6, "Rezero")},
    {scsi_command::eCmdRequestSense, make_pair(6, "RequestSense")},
    {scsi_command::eCmdFormatUnit, make_pair(6, "FormatUnit")},
    {scsi_command::eCmdReassignBlocks, make_pair(6, "ReassignBlocks")},
    {scsi_command::eCmdRead6, make_pair(6, "Read6/GetMessage10")},
    {scsi_command::eCmdRetrieveStats, make_pair(6, "RetrieveStats")},
    {scsi_command::eCmdWrite6, make_pair(6, "Write6/Print/SendMessage10")},
    {scsi_command::eCmdSeek6, make_pair(6, "Seek6")},
    {scsi_command::eCmdSetIfaceMode, make_pair(6, "SetIfaceMode")},
    {scsi_command::eCmdSetMcastAddr, make_pair(6, "SetMcastAddr")},
    {scsi_command::eCmdEnableInterface, make_pair(6, "EnableInterface")},
    {scsi_command::eCmdSynchronizeBuffer, make_pair(6, "SynchronizeBuffer")},
    {scsi_command::eCmdInquiry, make_pair(6, "Inquiry")},
    {scsi_command::eCmdModeSelect6, make_pair(6, "ModeSelect6")},
    {scsi_command::eCmdReserve6, make_pair(6, "Reserve6")},
    {scsi_command::eCmdRelease6, make_pair(6, "Release6")},
    {scsi_command::eCmdModeSense6, make_pair(6, "ModeSense6")},
    {scsi_command::eCmdStartStop, make_pair(6, "StartStop")},
    {scsi_command::eCmdStopPrint, make_pair(6, "StopPrint")},
    {scsi_command::eCmdSendDiagnostic, make_pair(6, "SendDiagnostic")},
    {scsi_command::eCmdPreventAllowMediumRemoval, make_pair(6, "PreventAllowMediumRemoval")},
    {scsi_command::eCmdReadCapacity10, make_pair(10, "ReadCapacity10")},
    {scsi_command::eCmdRead10, make_pair(10, "Read10")},
    {scsi_command::eCmdWrite10, make_pair(10, "Write10")},
    {scsi_command::eCmdSeek10, make_pair(10, "Seek10")},
    {scsi_command::eCmdVerify10, make_pair(10, "Verify10")},
    {scsi_command::eCmdSynchronizeCache10, make_pair(10, "SynchronizeCache10")},
    {scsi_command::eCmdReadDefectData10, make_pair(10, "ReadDefectData10")},
    {scsi_command::eCmdReadLong10, make_pair(10, "ReadLong10")},
    {scsi_command::eCmdWriteLong10, make_pair(10, "WriteLong10")},
    {scsi_command::eCmdReadToc, make_pair(10, "ReadToc")},
    {scsi_command::eCmdGetEventStatusNotification, make_pair(10, "GetEventStatusNotification")},
    {scsi_command::eCmdModeSelect10, make_pair(10, "ModeSelect10")},
    {scsi_command::eCmdModeSense10, make_pair(10, "ModeSense10")},
    {scsi_command::eCmdRead16, make_pair(16, "Read16")},
    {scsi_command::eCmdWrite16, make_pair(16, "Write16")},
    {scsi_command::eCmdVerify16, make_pair(16, "Verify16")},
    {scsi_command::eCmdSynchronizeCache16, make_pair(16, "SynchronizeCache16")},
    {scsi_command::eCmdReadCapacity16_ReadLong16, make_pair(16, "ReadCapacity16/ReadLong16")},
    {scsi_command::eCmdWriteLong16, make_pair(16, "WriteLong16")},
    {scsi_command::eCmdReportLuns, make_pair(12, "ReportLuns")}};
}; // namespace scsi_defs
