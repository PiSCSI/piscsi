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

using namespace rascsi_interface; //NOSONAR Not relevant for rascsi

class RascsiImage
{
public:

	RascsiImage();
	~RascsiImage() = default;

	void SetDepth(int d) { depth = d; }
	int GetDepth() const { return depth; }
	bool CheckDepth(string_view) const;
	bool CreateImageFolder(const CommandContext&, const string&) const;
	string GetDefaultImageFolder() const { return default_image_folder; }
	string SetDefaultImageFolder(const string&);
	bool IsValidSrcFilename(const string&) const;
	bool IsValidDstFilename(const string&) const;
	bool CreateImage(const CommandContext&, const PbCommand&) const;
	bool DeleteImage(const CommandContext&, const PbCommand&) const;
	bool RenameImage(const CommandContext&, const PbCommand&) const;
	bool CopyImage(const CommandContext&, const PbCommand&) const;
	bool SetImagePermissions(const CommandContext&, const PbCommand&) const;

private:

	string GetHomeDir() const;

	string default_image_folder;

	int depth = 1;
};
