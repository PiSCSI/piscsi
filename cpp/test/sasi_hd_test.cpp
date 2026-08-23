//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "devices/sasi_hd.h"
#include "shared/piscsi_exceptions.h"

using namespace scsi_defs;

TEST(SasiHdTest, Inquiry)
{
	auto [controller, hd] = CreateDevice(SAHD);

	EXPECT_CALL(*controller, DataIn());
	EXPECT_NO_THROW(hd->Dispatch(scsi_command::eCmdInquiry));
	EXPECT_EQ(2, controller->GetLength());
	EXPECT_EQ(0, controller->GetBuffer()[0]);
	EXPECT_EQ(0, controller->GetBuffer()[1]);
}

TEST(SasiHdTest, RequestSense)
{
	auto [controller, hd] = CreateDevice(SAHD);

	EXPECT_CALL(*controller, DataIn());
	EXPECT_NO_THROW(hd->Dispatch(scsi_command::eCmdRequestSense));
	EXPECT_EQ(4, controller->GetLength());
	EXPECT_EQ(0, controller->GetBuffer()[0]);
	EXPECT_EQ(0, controller->GetBuffer()[1]);
}

TEST(SasiHdTest, BlockSizesAndOpen)
{
	SasiHd hd(0);
	EXPECT_TRUE(hd.GetSupportedSectorSizes().contains(256));
	EXPECT_TRUE(hd.GetSupportedSectorSizes().contains(512));
	EXPECT_TRUE(hd.GetSupportedSectorSizes().contains(1024));

	EXPECT_THROW(hd.Open(), io_exception);
	const path filename = CreateTempFile(2048);
	hd.SetFilename(filename.string());
	hd.Open();
	EXPECT_EQ(256, hd.GetSectorSizeInBytes());
	EXPECT_EQ(8, hd.GetBlockCount());
}

TEST(SasiHdTest, ReadCapacity)
{
	auto [controller, device] = CreateDevice(SAHD);
	auto hd = dynamic_pointer_cast<SasiHd>(device);
	const path filename = CreateTempFile(2048);
	hd->SetFilename(filename.string());
	hd->Open();

	EXPECT_CALL(*controller, DataIn());
	EXPECT_NO_THROW(hd->Dispatch(scsi_command::eCmdReadBlockLimits));
	EXPECT_EQ(6, controller->GetLength());
	EXPECT_EQ(7, controller->GetBuffer()[3]);
	EXPECT_EQ(1, controller->GetBuffer()[4]);
	EXPECT_EQ(0, controller->GetBuffer()[5]);
}
