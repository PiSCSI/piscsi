//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
// Helper methods used by rascsi and rasctl
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include "rascsi_interface.pb.h"

bool GetAsInt(const std::string&, int&);
std::string ListDevices(const rascsi_interface::PbServerInfo&);
