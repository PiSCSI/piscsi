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

#include "sbc_version.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <iostream>
#include <sstream>

SBC_Version::sbc_version_type SBC_Version::m_sbc_version = sbc_version_type::sbc_unknown;

// TODO: THESE NEED TO BE VALIDATED!!!!
const std::string SBC_Version::m_str_raspberry_pi_1    = "Raspberry Pi 1";
const std::string SBC_Version::m_str_raspberry_pi_2_3  = "Raspberry Pi 2/3";
const std::string SBC_Version::m_str_raspberry_pi_4    = "Raspberry Pi 4";
const std::string SBC_Version::m_str_bananapi_m1_plus  = "Banana Pi M1 Plus";
const std::string SBC_Version::m_str_bananapi_m2_berry = "Banana Pi M2 Berry/Ultra";
const std::string SBC_Version::m_str_bananapi_m2_zero  = "Banana Pi M2 Zero";
const std::string SBC_Version::m_str_bananapi_m2_plus  = "Banana Pi BPI-M2-Plus H3";
const std::string SBC_Version::m_str_bananapi_m3       = "Banana Pi M3";
const std::string SBC_Version::m_str_bananapi_m4       = "Banana Pi M4";
const std::string SBC_Version::m_str_bananapi_m5       = "Banana Pi M5";
const std::string SBC_Version::m_str_bananapi_m64      = "Banana Pi M64";
const std::string SBC_Version::m_str_unknown_sbc       = "Unknown SBC";

// The strings in this table should align with the 'model' embedded
// in the device tree. This can be aquired by running:
//     cat /proc/device-tree/model
// Only the first part of the string is checked. Anything following
// will be ignored. For example:
//     "Raspberry Pi 4 Model B" will match with both of the following:
//         - Raspberry Pi 4 Model B Rev 1.4
//         - Raspberry Pi 4 Model B Rev 1.3
const std::map<std::string, SBC_Version::sbc_version_type, std::less<>> SBC_Version::m_proc_device_tree_mapping = {
    {"Raspberry Pi 1 Model ", sbc_version_type::sbc_raspberry_pi_1},
    {"Raspberry Pi 2 Model ", sbc_version_type::sbc_raspberry_pi_2_3},
    {"Raspberry Pi 3 Model ", sbc_version_type::sbc_raspberry_pi_2_3},
    {"Raspberry Pi 4 Model ", sbc_version_type::sbc_raspberry_pi_4},
    {"Raspberry Pi 400 ", sbc_version_type::sbc_raspberry_pi_4},
    {"Raspberry Pi Zero W", sbc_version_type::sbc_raspberry_pi_1},
    {"Raspberry Pi Zero", sbc_version_type::sbc_raspberry_pi_1},
    {"Banana Pi BPI-M2-Zero ", sbc_version_type::sbc_bananapi_m2_zero},
    {"Banana Pi BPI-M2-Ultra ", sbc_version_type::sbc_bananapi_m2_berry},
    {"Banana Pi BPI-M2-Plus H3", sbc_version_type::sbc_bananapi_m2_plus},
    {"Banana Pi M2 Berry ", sbc_version_type::sbc_bananapi_m2_berry},
    // sbc_bananapi_m3, TBD....
    // sbc_bananapi_m4,
};

const std::string SBC_Version::m_device_tree_model_path = "/proc/device-tree/model";

//---------------------------------------------------------------------------
//
//	Convert the SBC Version to a printable string
//
//---------------------------------------------------------------------------
std::string SBC_Version::GetString()
{
    switch (m_sbc_version) {
    case sbc_version_type::sbc_raspberry_pi_1:
        return m_str_raspberry_pi_1;
    case sbc_version_type::sbc_raspberry_pi_2_3:
        return m_str_raspberry_pi_2_3;
    case sbc_version_type::sbc_raspberry_pi_4:
        return m_str_raspberry_pi_4;
    case sbc_version_type::sbc_bananapi_m2_berry:
        return m_str_bananapi_m2_berry;
    case sbc_version_type::sbc_bananapi_m2_zero:
        return m_str_bananapi_m2_zero;
    case sbc_version_type::sbc_bananapi_m2_plus:
        return m_str_bananapi_m2_plus;
    case sbc_version_type::sbc_bananapi_m3:
        return m_str_bananapi_m3;
    case sbc_version_type::sbc_bananapi_m4:
        return m_str_bananapi_m4;
    default:
        spdlog::error("Unknown type of sbc detected: " + std::to_string(static_cast<int>(m_sbc_version)));
        return m_str_unknown_sbc;
    }
}

SBC_Version::sbc_version_type SBC_Version::GetSbcVersion()
{
    return m_sbc_version;
}

//---------------------------------------------------------------------------
//
//	Determine which version of single board computer (Pi) is being used
//  based upon the device tree model string.
//
//---------------------------------------------------------------------------
void SBC_Version::Init()
{
    spdlog::trace(__PRETTY_FUNCTION__);
    std::string device_tree_model;

    const std::ifstream input_stream(SBC_Version::m_device_tree_model_path);

    if (input_stream.fail()) {
#if defined(__x86_64__) || defined(__X86__)
        // We expect this to fail on x86
        spdlog::info("Detected device " + GetString());
        m_sbc_version = sbc_version_type::sbc_unknown;
        return;
#else
        spdlog::error("Failed to open " + SBC_Version::m_device_tree_model_path + ". Are you running as root?");
        throw std::invalid_argument("Failed to open /proc/device-tree/model");
#endif
    }

    std::stringstream str_buffer;
    str_buffer << input_stream.rdbuf();
    device_tree_model = str_buffer.str();

    for (const auto &[key, value] : m_proc_device_tree_mapping) {
        if (device_tree_model.rfind(key, 0) == 0) {
            m_sbc_version = value;
            spdlog::info("Detected device " + GetString());
            return;
        }
    }
    spdlog::error("Unable to determine single board computer type. Defaulting to Raspberry Pi 4");
    m_sbc_version = sbc_version_type::sbc_raspberry_pi_4;
}

bool SBC_Version::IsRaspberryPi()
{
    spdlog::trace(__PRETTY_FUNCTION__);
    switch (m_sbc_version) {
    case sbc_version_type::sbc_raspberry_pi_1:
    case sbc_version_type::sbc_raspberry_pi_2_3:
    case sbc_version_type::sbc_raspberry_pi_4:
        return true;
    case sbc_version_type::sbc_bananapi_m2_berry:
    case sbc_version_type::sbc_bananapi_m2_zero:
    case sbc_version_type::sbc_bananapi_m2_plus:
    case sbc_version_type::sbc_bananapi_m3:
    case sbc_version_type::sbc_bananapi_m4:
        return false;
    default:
        return false;
    }
}

bool SBC_Version::IsBananaPi()
{
	   spdlog::trace(__PRETTY_FUNCTION__);
    switch (m_sbc_version) {
    case sbc_version_type::sbc_raspberry_pi_1:
    case sbc_version_type::sbc_raspberry_pi_2_3:
    case sbc_version_type::sbc_raspberry_pi_4:
        return false;
    case sbc_version_type::sbc_bananapi_m2_berry:
    case sbc_version_type::sbc_bananapi_m2_zero:
    case sbc_version_type::sbc_bananapi_m2_plus:
    case sbc_version_type::sbc_bananapi_m3:
    case sbc_version_type::sbc_bananapi_m4:
        return true;
    default:
        return false;
    }
}

// The following functions are only used on the Raspberry Pi
//	(imported from bcm_host.c)
uint32_t SBC_Version::GetDeviceTreeRanges(const char *filename, uint32_t offset)
{
	   spdlog::trace(__PRETTY_FUNCTION__);
    uint32_t address = ~0;
    if (FILE *fp = fopen(filename, "rb"); fp) {
        fseek(fp, offset, SEEK_SET);
        if (std::array<uint8_t, 4> buf; fread(buf.data(), 1, buf.size(), fp) == buf.size()) {
            address = (int)buf[0] << 24 | (int)buf[1] << 16 | (int)buf[2] << 8 | (int)buf[3] << 0;
        }
        fclose(fp);
    }
    return address;
}

#if defined __linux__
uint32_t SBC_Version::GetPeripheralAddress(void)
{
	   spdlog::trace(__PRETTY_FUNCTION__);
    uint32_t address = GetDeviceTreeRanges("/proc/device-tree/soc/ranges", 4);
    if (address == 0) {
        address = GetDeviceTreeRanges("/proc/device-tree/soc/ranges", 8);
    }
    address = (address == (uint32_t)~0) ? 0x20000000 : address;

    spdlog::debug("Peripheral address: " + std::to_string(address));

    return address;
}
#elif defined __NetBSD__
uint32_t SBC_Version::GetPeripheralAddress(void)
{
    char buf[1024];
    size_t len = sizeof(buf);
    uint32_t address;

    if (sysctlbyname("hw.model", buf, &len, NULL, 0) || strstr(buf, "ARM1176JZ-S") != buf) {
        // Failed to get CPU model || Not BCM2835
        // use the address of BCM283[67]
        address = 0x3f000000;
    } else {
        // Use BCM2835 address
        address = 0x20000000;
    }
    spdlog::debug("Peripheral address: " + std::to_string(address));
    return address;
}
#else
uint32_t SBC_Version::GetPeripheralAddress(void)
{
    return 0;
}
#endif
