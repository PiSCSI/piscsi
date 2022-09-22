//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "testing.h"
#include "devices/device_factory.h"
#include "rascsi_interface.pb.h"
#include "rascsi_response.h"
#include "rascsi_image.h"

using namespace rascsi_interface;

TEST(ResponseTest, Operation_Count)
{
	DeviceFactory device_factory;
	RascsiImage rascsi_image;
	RascsiResponse rascsi_response(&device_factory, &rascsi_image);
	PbResult pb_operation_info_result;

	const auto operation_info = unique_ptr<PbOperationInfo>(rascsi_response.GetOperationInfo(pb_operation_info_result, 0));
	EXPECT_EQ(PbOperation_ARRAYSIZE - 1, operation_info->operations_size());
}
