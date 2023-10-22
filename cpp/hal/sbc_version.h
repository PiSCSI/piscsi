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
        sbc_raspberry_pi_4,
        sbc_bananapi_m1_plus,
        sbc_bananapi_m2_ultra,
        sbc_bananapi_m2_berry,
        sbc_bananapi_m2_zero,
        sbc_bananapi_m2_plus,
        sbc_bananapi_m3,
        sbc_bananapi_m4,
        sbc_bananapi_m5,
        sbc_bananapi_m64,
    };

    SBC_Version()  = delete;
    ~SBC_Version() = delete;

    static void Init();

    static sbc_version_type GetSbcVersion();

    static bool IsRaspberryPi();
    static bool IsBananaPi();

    static const std::string *GetString();

    static uint32_t GetPeripheralAddress();

  private:
    static sbc_version_type m_sbc_version;

    static const std::string m_str_raspberry_pi_1;
    static const std::string m_str_raspberry_pi_2_3;
    static const std::string m_str_raspberry_pi_4;
    static const std::string m_str_bananapi_m1_plus;
    static const std::string m_str_bananapi_m2_ultra;
    static const std::string m_str_bananapi_m2_berry;
    static const std::string m_str_bananapi_m2_zero;
    static const std::string m_str_bananapi_m2_plus;
    static const std::string m_str_bananapi_m3;
    static const std::string m_str_bananapi_m4;
    static const std::string m_str_bananapi_m5;
    static const std::string m_str_bananapi_m64;
    static const std::string m_str_unknown_sbc;

    static const std::map<std::string, sbc_version_type, std::less<>> m_proc_device_tree_mapping;

    static const std::string m_device_tree_model_path;

    static uint32_t GetDeviceTreeRanges(const char *filename, uint32_t offset);
};
