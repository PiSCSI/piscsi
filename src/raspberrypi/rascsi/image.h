//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "rascsi_interface.pb.h"
#include "command_context.h"
#include <string>

using namespace rascsi_interface;

class RascsiImage
{
public:

	RascsiImage();
	~RascsiImage() = default;

	void SetDepth(int d) { depth = d; }
	int GetDepth() const { return depth; }
	bool CheckDepth(string_view) const;
	bool CreateImageFolder(CommandContext&, const string&) const;
	string GetDefaultFolder() const { return default_folder; }
	string SetDefaultFolder(const string&);
	bool IsValidSrcFilename(const string&) const;
	bool IsValidDstFilename(const string&) const;
	bool CreateImage(CommandContext&, const PbCommand&) const;
	bool DeleteImage(CommandContext&, const PbCommand&) const;
	bool RenameImage(CommandContext&, const PbCommand&) const;
	bool CopyImage(CommandContext&, const PbCommand&) const;
	bool SetImagePermissions(CommandContext&, const PbCommand&) const;
	string GetFullName(const string& filename) const { return default_folder + "/" + filename; }

private:

	string GetHomeDir() const;

	string default_folder;

	int depth = 1;
};
