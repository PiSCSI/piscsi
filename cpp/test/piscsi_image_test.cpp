//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/protobuf_util.h"
#include "generated/piscsi_interface.pb.h"
#include "piscsi/piscsi_image.h"

using namespace piscsi_interface;
using namespace protobuf_util;

TEST(PiscsiImageTest, SetGetDepth)
{
	PiscsiImage image;

	image.SetDepth(1);
	EXPECT_EQ(1, image.GetDepth());
}

TEST(PiscsiImageTest, SetGetDefaultFolder)
{
	PiscsiImage image;

	EXPECT_NE(string::npos, image.GetDefaultFolder().find("/images"));

	EXPECT_TRUE(!image.SetDefaultFolder("").empty());
	EXPECT_TRUE(!image.SetDefaultFolder("/not_in_home").empty());
}

TEST(PiscsiImageTest, CreateImage)
{
	MockCommandContext context;
	PbCommand command;
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	EXPECT_FALSE(image.CreateImage(context, command)) << "Filename must be reported as missing";

	SetParam(command, "file", "/a/b/c/filename");
	EXPECT_FALSE(image.CreateImage(context, command)) << "Depth must be reported as invalid";

	SetParam(command, "file", "filename");
	SetParam(command, "size", "-1");
	EXPECT_FALSE(image.CreateImage(context, command)) << "Size must be reported as invalid";

	SetParam(command, "size", "1");
	EXPECT_FALSE(image.CreateImage(context, command)) << "Size must be reported as invalid";

	SetParam(command, "size", "513");
	EXPECT_FALSE(image.CreateImage(context, command)) << "Size must be reported as not a multiple of 512";

	// Further tests would modify the filesystem
}

TEST(PiscsiImageTest, DeleteImage)
{
	MockCommandContext context;
	PbCommand command;
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	EXPECT_FALSE(image.DeleteImage(context, command)) << "Filename must be reported as missing";

	SetParam(command, "file", "/a/b/c/filename");
	EXPECT_FALSE(image.DeleteImage(context, command)) << "Depth must be reported as invalid";

	MockStorageDevice device;
	device.ReserveFile("filename", 0, 0);
	SetParam(command, "file", "filename");
	EXPECT_FALSE(image.DeleteImage(context, command)) << "File must be reported as in use";

	// Further testing would modify the filesystem
}

TEST(PiscsiImageTest, RenameImage)
{
	MockCommandContext context;
	PbCommand command;
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	EXPECT_FALSE(image.RenameImage(context, command)) << "Source filename must be reported as missing";

	SetParam(command, "from", "/a/b/c/filename_from");
	EXPECT_FALSE(image.RenameImage(context, command)) << "Depth must be reported as invalid";

	SetParam(command, "from", "filename_from");
	EXPECT_FALSE(image.RenameImage(context, command)) << "Source file must be reported as missing";

	// Further testing would modify the filesystem
}

TEST(PiscsiImageTest, CopyImage)
{
	MockCommandContext context;
	PbCommand command;
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	EXPECT_FALSE(image.CopyImage(context, command)) << "Source filename must be reported as missing";

	SetParam(command, "from", "/a/b/c/filename_from");
	EXPECT_FALSE(image.CopyImage(context, command)) << "Depth must be reported as invalid";

	SetParam(command, "from", "filename_from");
	EXPECT_FALSE(image.CopyImage(context, command)) << "Source file must be reported as missing";

	// Further testing would modify the filesystem
}

TEST(PiscsiImageTest, SetImagePermissions)
{
	MockCommandContext context;
	PbCommand command;
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	EXPECT_FALSE(image.SetImagePermissions(context, command)) << "Filename must be reported as missing";

	SetParam(command, "file", "/a/b/c/filename");
	EXPECT_FALSE(image.SetImagePermissions(context, command)) << "Depth must be reported as invalid";

	SetParam(command, "file", "filename");
	EXPECT_FALSE(image.CopyImage(context, command)) << "Source file must be reported as missing";

	// Further testing would modify the filesystem
}
