//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//  Copyright (C) 2022 akuker
//
//	[ Hardware version detection routines ]
//
//---------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include <map>
#include <string>

using namespace std;

//===========================================================================
//
//	Single Board Computer Versions
//
//===========================================================================
class SBC_Version
{
public:
	// Type of Single Board Computer
	enum class sbc_version_type : uint8_t {
		sbc_unknown = 0,
		sbc_raspberry_pi_1,
		sbc_raspberry_pi_2_3,
		sbc_raspberry_pi_4
	};

	SBC_Version()  = delete;
	~SBC_Version() = delete;

	static void Init();

	static sbc_version_type GetSbcVersion();

	static bool IsRaspberryPi();

	static string GetAsString();

	static uint32_t GetPeripheralAddress();

private:
	static sbc_version_type sbc_version;

	static const string str_raspberry_pi_1;
	static const string str_raspberry_pi_2_3;
	static const string str_raspberry_pi_4;
	static const string str_unknown_sbc;

	static const map<std::string, sbc_version_type, less<>> proc_device_tree_mapping;

	static const string m_device_tree_model_path;

	static uint32_t GetDeviceTreeRanges(const char *filename, uint32_t offset);
};
