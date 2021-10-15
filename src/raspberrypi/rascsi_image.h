//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI (*^..^*)
// for Raspberry Pi
//
// Copyright (C) 2021 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "rascsi_interface.pb.h"
#include <string>

using namespace std;
using namespace rascsi_interface;

class RascsiImage
{
public:

	RascsiImage() {};
	~RascsiImage() {};

	const string& GetDefaultImageFolder() const { return default_image_folder; }
	void SetDefaultImageFolder(const string& default_image_folder) { this->default_image_folder = default_image_folder; }
	bool IsValidSrcFilename(const string&);
	bool IsValidDstFilename(const string&);
	bool CreateImage(int, const PbCommand&);
	bool DeleteImage(int, const PbCommand&);
	bool RenameImage(int, const PbCommand&);
	bool CopyImage(int, const PbCommand&);
	bool SetImagePermissions(int, const PbCommand&);

private:

	string default_image_folder;
};
