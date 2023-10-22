//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
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
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	PbCommand command1;
	CommandContext context1(command1, "", "");
	EXPECT_FALSE(image.CreateImage(context1)) << "Filename must be reported as missing";

	PbCommand command2;
	SetParam(command2, "file", "/a/b/c/filename");
	CommandContext context2(command2, "", "");
	EXPECT_FALSE(image.CreateImage(context2)) << "Depth must be reported as invalid";

	PbCommand command3;
	SetParam(command3, "file", "filename");
	SetParam(command3, "size", "-1");
	CommandContext context3(command3, "", "");
	EXPECT_FALSE(image.CreateImage(context3)) << "Size must be reported as invalid";

	PbCommand command4;
	SetParam(command4, "size", "1");
	CommandContext context4(command4, "", "");
	EXPECT_FALSE(image.CreateImage(context4)) << "Size must be reported as invalid";

	PbCommand command5;
	SetParam(command5, "size", "513");
	CommandContext context5(command5, "", "");
	EXPECT_FALSE(image.CreateImage(context4)) << "Size must be reported as not a multiple of 512";

	// Further tests would modify the filesystem
}

TEST(PiscsiImageTest, DeleteImage)
{
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	PbCommand command1;
	CommandContext context1(command1, "", "");
	EXPECT_FALSE(image.DeleteImage(context1)) << "Filename must be reported as missing";

	PbCommand command2;
	SetParam(command2, "file", "/a/b/c/filename");
	CommandContext context2(command2, "", "");
	EXPECT_FALSE(image.DeleteImage(context2)) << "Depth must be reported as invalid";

	MockStorageDevice device;
	device.SetFilename("filename");
	device.ReserveFile();
	PbCommand command3;
	SetParam(command3, "file", "filename");
	CommandContext context3(command3, "", "");
	EXPECT_FALSE(image.DeleteImage(context3)) << "File must be reported as in use";

	// Further testing would modify the filesystem
}

TEST(PiscsiImageTest, RenameImage)
{
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	PbCommand command1;
	CommandContext context1(command1, "", "");
	EXPECT_FALSE(image.RenameImage(context1)) << "Source filename must be reported as missing";

	PbCommand command2;
	SetParam(command2, "from", "/a/b/c/filename_from");
	CommandContext context2(command2, "", "");
	EXPECT_FALSE(image.RenameImage(context2)) << "Depth must be reported as invalid";

	PbCommand command3;
	SetParam(command3, "from", "filename_from");
	CommandContext context3(command3, "", "");
	EXPECT_FALSE(image.RenameImage(context3)) << "Source file must be reported as missing";

	// Further testing would modify the filesystem
}

TEST(PiscsiImageTest, CopyImage)
{
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	PbCommand command1;
	CommandContext context1(command1, "", "");
	EXPECT_FALSE(image.CopyImage(context1)) << "Source filename must be reported as missing";

	PbCommand command2;
	SetParam(command2, "from", "/a/b/c/filename_from");
	CommandContext context2(command2, "", "");
	EXPECT_FALSE(image.CopyImage(context2)) << "Depth must be reported as invalid";

	PbCommand command3;
	SetParam(command3, "from", "filename_from");
	CommandContext context3(command3, "", "");
	EXPECT_FALSE(image.CopyImage(context3)) << "Source file must be reported as missing";

	// Further testing would modify the filesystem
}

TEST(PiscsiImageTest, SetImagePermissions)
{
	PiscsiImage image;

	StorageDevice::UnreserveAll();

	PbCommand command1;
	CommandContext context1(command1, "", "");
	EXPECT_FALSE(image.SetImagePermissions(context1)) << "Filename must be reported as missing";

	PbCommand command2;
	SetParam(command2, "file", "/a/b/c/filename");
	CommandContext context2(command2, "", "");
	EXPECT_FALSE(image.SetImagePermissions(context2)) << "Depth must be reported as invalid";

	PbCommand command3;
	SetParam(command3, "file", "filename");
	CommandContext context3(command3, "", "");
	EXPECT_FALSE(image.CopyImage(context3)) << "Source file must be reported as missing";

	// Further testing would modify the filesystem
}
