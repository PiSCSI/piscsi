//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/piscsi_interface.pb.h"
#include "command_context.h"
#include <string>
#include <filesystem>

using namespace std;
using namespace filesystem;
using namespace piscsi_interface;

class PiscsiImage
{
public:

	PiscsiImage();
	~PiscsiImage() = default;

	void SetDepth(int d) { depth = d; }
	int GetDepth() const { return depth; }
	string GetDefaultFolder() const { return default_folder; }
	string SetDefaultFolder(string_view);
	bool CreateImage(const CommandContext&) const;
	bool DeleteImage(const CommandContext&) const;
	bool RenameImage(const CommandContext&) const;
	bool CopyImage(const CommandContext&) const;
	bool SetImagePermissions(const CommandContext&) const;

private:

	bool CheckDepth(string_view) const;
	string GetFullName(const string& filename) const { return default_folder + "/" + filename; }
	bool CreateImageFolder(const CommandContext&, string_view) const;
	bool ValidateParams(const CommandContext&, const string&, string&, string&) const;

	static bool IsValidSrcFilename(string_view);
	static bool IsValidDstFilename(string_view);
	static bool ChangeOwner(const CommandContext&, const path&, bool);
	static string GetHomeDir();
	static pair<int, int> GetUidAndGid();

	string default_folder;

	int depth = 1;
};
