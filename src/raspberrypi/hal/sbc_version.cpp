//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
//	for Raspberry Pi
//
//  Copyright (C) 2022 akuker
//
//	[ Hardware version detection routines ]
//
//---------------------------------------------------------------------------

#include "sbc_version.h"
#include "log.h"
#include <fstream>
#include <iostream>
#include <sstream>

SBC_Version::sbc_version_type SBC_Version::m_sbc_version = SBC_Version::sbc_unknown;

const std::string SBC_Version::m_str_raspberry_pi_1    = "Raspberry Pi 1";
const std::string SBC_Version::m_str_raspberry_pi_2_3  = "Raspberry Pi 2/3";
const std::string SBC_Version::m_str_raspberry_pi_4    = "Raspberry Pi 4";
const std::string SBC_Version::m_str_bananapi_m2_berry = "Banana Pi M2 Berry/Ultra";
const std::string SBC_Version::m_str_bananapi_m2_zero  = "Banana Pi M2 Zero";
const std::string SBC_Version::m_str_bananapi_m2_plus  = "Banana Pi BPI-M2-Plus H3";
const std::string SBC_Version::m_str_bananapi_m3       = "Banana Pi M3";
const std::string SBC_Version::m_str_bananapi_m4       = "Banana Pi M4";
const std::string SBC_Version::m_str_unknown_sbc       = "Unknown SBC";

// The strings in this table should align with the 'model' embedded
// in the device tree. This can be aquired by running:
//     cat /proc/device-tree/model
// Only the first part of the string is checked. Anything following
// will be ignored. For example:
//     "Raspberry Pi 4 Model B" will match with both of the following:
//         - Raspberry Pi 4 Model B Rev 1.4
//         - Raspberry Pi 4 Model B Rev 1.3
const std::map<std::string, SBC_Version::sbc_version_type> SBC_Version::m_proc_device_tree_mapping = {
    {"Raspberry Pi 1 Model ", SBC_Version::sbc_raspberry_pi_1},
    {"Raspberry Pi 2 Model ", SBC_Version::sbc_raspberry_pi_2_3},
    {"Raspberry Pi 3 Model ", SBC_Version::sbc_raspberry_pi_2_3},
    {"Raspberry Pi 4 Model ", SBC_Version::sbc_raspberry_pi_4},
    {"Raspberry Pi 400 ", SBC_Version::sbc_raspberry_pi_4},
    {"Raspberry Pi Zero W", SBC_Version::sbc_raspberry_pi_1},
    {"Raspberry Pi Zero", SBC_Version::sbc_raspberry_pi_1},
    {"Banana Pi BPI-M2-Zero ", SBC_Version::sbc_bananapi_m2_zero},
    {"Banana Pi BPI-M2-Ultra ", SBC_Version::sbc_bananapi_m2_berry},
    {"Banana Pi BPI-M2-Plus H3", SBC_Version::sbc_bananapi_m2_plus},
    {"Banana Pi M2 Berry ", SBC_Version::sbc_bananapi_m2_berry},
    // sbc_bananapi_m3, TBD....
    // sbc_bananapi_m4,
};

const std::string SBC_Version::m_device_tree_model_path = "/proc/device-tree/model";

//---------------------------------------------------------------------------
//
//	Convert the SBC Version to a printable string
//
//---------------------------------------------------------------------------
const std::string *SBC_Version::GetString()
{
    switch (m_sbc_version) {
    case sbc_raspberry_pi_1:
        return &m_str_raspberry_pi_1;
    case sbc_raspberry_pi_2_3:
        return &m_str_raspberry_pi_2_3;
    case sbc_raspberry_pi_4:
        return &m_str_raspberry_pi_4;
    case sbc_bananapi_m2_berry:
        return &m_str_bananapi_m2_berry;
    case sbc_bananapi_m2_zero:
        return &m_str_bananapi_m2_zero;
    case sbc_bananapi_m2_plus:
        return &m_str_bananapi_m2_plus;
    case sbc_bananapi_m3:
        return &m_str_bananapi_m3;
    case sbc_bananapi_m4:
        return &m_str_bananapi_m4;
    default:
        LOGERROR("Unknown type of sbc detected: %d", m_sbc_version);
        return &m_str_unknown_sbc;
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
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    std::string device_tree_model;

    const std::ifstream input_stream(SBC_Version::m_device_tree_model_path);

    if (input_stream.fail()) {
        LOGERROR("Failed to open %s. Are you running as root?", SBC_Version::m_device_tree_model_path.c_str());
        throw std::runtime_error("Failed to open /proc/device-tree/model");
    }

    std::stringstream str_buffer;
    str_buffer << input_stream.rdbuf();
    device_tree_model = str_buffer.str();

    for (const auto &[key, value] : m_proc_device_tree_mapping) {
        if (device_tree_model.rfind(key, 0) == 0) {
            m_sbc_version = value;
            LOGINFO("Detected device %s", GetString()->c_str());
            return;
        }
    }
    LOGERROR("%s Unable to determine single board computer type. Defaulting to Raspberry Pi 4", __PRETTY_FUNCTION__);
    m_sbc_version = sbc_version_type::sbc_raspberry_pi_4;
}

bool SBC_Version::IsRaspberryPi()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    switch (m_sbc_version) {
    case sbc_raspberry_pi_1:
    case sbc_raspberry_pi_2_3:
    case sbc_raspberry_pi_4:
        return true;
    case sbc_bananapi_m2_berry:
    case sbc_bananapi_m2_zero:
    case sbc_bananapi_m2_plus:
    case sbc_bananapi_m3:
    case sbc_bananapi_m4:
        return false;
    default:
        return false;
    }
}

bool SBC_Version::IsBananaPi()
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    switch (m_sbc_version) {
    case sbc_raspberry_pi_1:
    case sbc_raspberry_pi_2_3:
    case sbc_raspberry_pi_4:
        return false;
    case sbc_bananapi_m2_berry:
    case sbc_bananapi_m2_zero:
    case sbc_bananapi_m2_plus:
    case sbc_bananapi_m3:
    case sbc_bananapi_m4:
        return true;
    default:
        return false;
    }
}

// The following functions are only used on the Raspberry Pi
//	(imported from bcm_host.c)
DWORD SBC_Version::GetDeviceTreeRanges(const char *filename, DWORD offset)
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    DWORD address = ~0;
    if (FILE *fp = fopen(filename, "rb"); fp) {
        fseek(fp, offset, SEEK_SET);
        if (BYTE buf[4]; fread(buf, 1, sizeof buf, fp) == sizeof buf) {
            address = buf[0] << 24 | buf[1] << 16 | buf[2] << 8 | buf[3] << 0;
        }
        fclose(fp);
    }
    return address;
}

#if defined __linux__
DWORD SBC_Version::GetPeripheralAddress(void)
{
    LOGTRACE("%s", __PRETTY_FUNCTION__);
    DWORD address = GetDeviceTreeRanges("/proc/device-tree/soc/ranges", 4);
    if (address == 0) {
        address = GetDeviceTreeRanges("/proc/device-tree/soc/ranges", 8);
    }
    address = (address == (DWORD)~0) ? 0x20000000 : address;

    LOGDEBUG("Peripheral address : 0x%8x\n", address);

    return address;
}
#elif defined __NetBSD__
DWORD SBC_Version::GetPeripheralAddress(void)
{
    char buf[1024];
    size_t len = sizeof(buf);
    DWORD address;

    if (sysctlbyname("hw.model", buf, &len, NULL, 0) || strstr(buf, "ARM1176JZ-S") != buf) {
        // Failed to get CPU model || Not BCM2835
        // use the address of BCM283[67]
        address = 0x3f000000;
    } else {
        // Use BCM2835 address
        address = 0x20000000;
    }
    LOGDEBUG("Peripheral address : 0x%lx\n", address);
    return address;
}
#else
DWORD SBC_Version::GetPeripheralAddress(void)
{
    return 0;
}
#endif