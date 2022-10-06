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

using namespace std;
using namespace rascsi_interface;

class RascsiImage
{
public:

	RascsiImage();
	~RascsiImage() = default;

	void SetDepth(int d) { depth = d; }
	int GetDepth() const { return depth; }
	string GetDefaultFolder() const { return default_folder; }
	string SetDefaultFolder(const string&);
	bool CreateImage(const CommandContext&, const PbCommand&) const;
	bool DeleteImage(const CommandContext&, const PbCommand&) const;
	bool RenameImage(const CommandContext&, const PbCommand&) const;
	bool CopyImage(const CommandContext&, const PbCommand&) const;
	bool SetImagePermissions(const CommandContext&, const PbCommand&) const;

private:

	bool CheckDepth(string_view) const;
	string GetFullName(const string& filename) const { return default_folder + "/" + filename; }
	bool CreateImageFolder(const CommandContext&, const string&) const;
	bool IsValidSrcFilename(const string&) const;
	bool IsValidDstFilename(const string&) const;
	bool ValidateParams(const CommandContext&, const PbCommand&, const string&, string&, string&) const;

	static string GetHomeDir();

	string default_folder;

	int depth = 1;
};
