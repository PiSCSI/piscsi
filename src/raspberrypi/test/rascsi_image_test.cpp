//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "protobuf_util.h"
#include "rascsi_interface.pb.h"
#include "rascsi/rascsi_image.h"

using namespace rascsi_interface;
using namespace protobuf_util;

TEST(RascsiImageTest, SetGetDepth)
{
	RascsiImage image;

	image.SetDepth(1);
	EXPECT_EQ(1, image.GetDepth());
}

TEST(RascsiImageTest, SetGetDefaultFolder)
{
	RascsiImage image;

	EXPECT_NE(string::npos, image.GetDefaultFolder().find("/images"));

	EXPECT_TRUE(!image.SetDefaultFolder("").empty());
	EXPECT_TRUE(!image.SetDefaultFolder("/not_in_home").empty());
}

TEST(RascsiImageTest, CreateImage)
{
	MockCommandContext context;
	PbCommand command;
	RascsiImage image;

	EXPECT_FALSE(image.CreateImage(context, command)) << "Filename must be reported as missing";

	SetParam(command, "file", "/a/b/c/filename");
	EXPECT_FALSE(image.CreateImage(context, command)) << "Depth must be reported as invalid";

	SetParam(command, "file", "filename");
	SetParam(command, "size", "-1");
	EXPECT_FALSE(image.CreateImage(context, command)) << "Size must be reported as invalid";

	SetParam(command, "size", "1");
	EXPECT_FALSE(image.CreateImage(context, command)) << "Size must be reported as invalid";

	SetParam(command, "size", "513");
	EXPECT_FALSE(image.CreateImage(context, command)) << "Size must be reported as invalid";

	// Further tests would modify the filesystem
}

TEST(RascsiImageTest, DeleteImage)
{
	MockCommandContext context;
	PbCommand command;
	RascsiImage image;

	EXPECT_FALSE(image.DeleteImage(context, command)) << "Filename must be reported as missing";

	SetParam(command, "file", "/a/b/c/filename");
	EXPECT_FALSE(image.DeleteImage(context, command)) << "Depth must be reported as invalid";

	// Further testing would modify the filesystem
}

TEST(RascsiImageTest, RenameImage)
{
	MockCommandContext context;
	PbCommand command;
	RascsiImage image;

	EXPECT_FALSE(image.RenameImage(context, command)) << "Filenames must be reported as missing";

	SetParam(command, "to", "/a/b/c/filename_to");
	EXPECT_FALSE(image.RenameImage(context, command)) << "Depth must be reported as invalid";

	SetParam(command, "to", "filename_to");
	EXPECT_FALSE(image.RenameImage(context, command)) << "Source filename must be reported as missing";

	SetParam(command, "from", "/a/b/c/filename_from");
	EXPECT_FALSE(image.RenameImage(context, command)) << "Depth must be reported as invalid";

	// Further testing would modify the filesystem
}

TEST(RascsiImageTest, CopyImage)
{
	MockCommandContext context;
	PbCommand command;
	RascsiImage image;

	EXPECT_FALSE(image.CopyImage(context, command)) << "Filenames must be reported as missing";

	SetParam(command, "to", "/a/b/c/filename_to");
	EXPECT_FALSE(image.CopyImage(context, command)) << "Depth must be reported as invalid";

	SetParam(command, "to", "filename_to");
	EXPECT_FALSE(image.CopyImage(context, command)) << "Source filename must be reported as missing";

	SetParam(command, "from", "/a/b/c/filename_from");
	EXPECT_FALSE(image.CopyImage(context, command)) << "Depth must be reported as invalid";

	// Further testing would modify the filesystem
}

TEST(RascsiImageTest, SetImagePermissions)
{
	MockCommandContext context;
	PbCommand command;
	RascsiImage image;

	EXPECT_FALSE(image.SetImagePermissions(context, command)) << "Filename must be reported as missing";

	SetParam(command, "file", "/a/b/c/filename");
	EXPECT_FALSE(image.SetImagePermissions(context, command)) << "Depth must be reported as invalid";

	SetParam(command, "file", "filename");
	EXPECT_FALSE(image.CopyImage(context, command)) << "Source file must be reported as missing";

	// Further testing would modify the filesystem
}
