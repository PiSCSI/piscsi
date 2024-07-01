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
#include "rpi_revision_code.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <iostream>
#include <sstream>

SBC_Version::sbc_version_type SBC_Version::sbc_version = sbc_version_type::sbc_unknown;

const string SBC_Version::str_raspberry_pi_1 = "Raspberry Pi 1";
const string SBC_Version::str_raspberry_pi_2_3 = "Raspberry Pi 2/3";
const string SBC_Version::str_raspberry_pi_4 = "Raspberry Pi 4";
const string SBC_Version::str_raspberry_pi_5 = "Raspberry Pi 5";
const string SBC_Version::str_unknown_sbc = "Unknown SBC";

const string SBC_Version::m_device_tree_model_path = "/proc/device-tree/model";

//---------------------------------------------------------------------------
//
//	Convert the SBC Version to a printable string
//
//---------------------------------------------------------------------------
string SBC_Version::GetAsString()
{
    switch (sbc_version)
    {
    case sbc_version_type::sbc_raspberry_pi_1:
        return str_raspberry_pi_1;
    case sbc_version_type::sbc_raspberry_pi_2_3:
        return str_raspberry_pi_2_3;
    case sbc_version_type::sbc_raspberry_pi_4:
        return str_raspberry_pi_4;
    case sbc_version_type::sbc_raspberry_pi_5:
        return str_raspberry_pi_5;
    default:
        return str_unknown_sbc;
    }
}

SBC_Version::sbc_version_type SBC_Version::GetSbcVersion()
{
    return sbc_version;
}

SBC_Version::sbc_version_type SBC_Version::rpi_rev_to_sbc_version(Rpi_Revision_Code::rpi_version_type rpi_code)
{

    switch (rpi_code)
    {

    case Rpi_Revision_Code::rpi_version_type::rpi_version_A:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_B:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_Aplus:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_Bplus:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_Alpha: // (early prototype)
    case Rpi_Revision_Code::rpi_version_type::rpi_version_Zero:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_ZeroW:
        return sbc_version_type::sbc_raspberry_pi_1;
    case Rpi_Revision_Code::rpi_version_type::rpi_version_2B:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_CM1:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_3B:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_CM3:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_Zero2W:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_3Bplus:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_3Aplus:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_CM3plus:
        return sbc_version_type::sbc_raspberry_pi_2_3;
    case Rpi_Revision_Code::rpi_version_type::rpi_version_4B:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_400:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_CM4:
    case Rpi_Revision_Code::rpi_version_type::rpi_version_CM4S:
        return sbc_version_type::sbc_raspberry_pi_4;
    case Rpi_Revision_Code::rpi_version_type::rpi_version_5:
        return sbc_version_type::sbc_raspberry_pi_5;
    default:
        return sbc_version_type::sbc_unknown;
    }
}

//---------------------------------------------------------------------------
//
//	Determine which version of single board computer (Pi) is being used
//  based upon the device tree model string.
//
//---------------------------------------------------------------------------
void SBC_Version::Init()
{
    spdlog::error("Checking rpi version...");
    auto rpi_version = Rpi_Revision_Code();
    if (rpi_version.IsValid())
    {
        spdlog::debug("Detected valid raspberry pi version");
    }
    else
    {
        spdlog::error("Invalid rpi version defined");
    }

    sbc_version = rpi_rev_to_sbc_version(rpi_version.Type());
 

    if (sbc_version == sbc_version_type::sbc_unknown){
        sbc_version = sbc_version_type::sbc_raspberry_pi_4;
        spdlog::error("Unable to determine single board computer type. Defaulting to Raspberry Pi 4");
    }
}

bool SBC_Version::IsRaspberryPi()
{
    switch (sbc_version)
    {
    case sbc_version_type::sbc_raspberry_pi_1:
    case sbc_version_type::sbc_raspberry_pi_2_3:
    case sbc_version_type::sbc_raspberry_pi_4:
        return true;
    default:
        return false;
    }
}

// The following functions are only used on the Raspberry Pi
//	(imported from bcm_host.c)
uint32_t SBC_Version::GetDeviceTreeRanges(const char *filename, uint32_t offset)
{
    uint32_t address = ~0;
    if (FILE *fp = fopen(filename, "rb"); fp)
    {
        fseek(fp, offset, SEEK_SET);
        if (array<uint8_t, 4> buf; fread(buf.data(), 1, buf.size(), fp) == buf.size())
        {
            address = (int)buf[0] << 24 | (int)buf[1] << 16 | (int)buf[2] << 8 | (int)buf[3] << 0;
        }
        fclose(fp);
    }
    return address;
}

#if defined __linux__
uint32_t SBC_Version::GetPeripheralAddress(void)
{
    uint32_t address = GetDeviceTreeRanges("/proc/device-tree/soc/ranges", 4);
    if (address == 0)
    {
        address = GetDeviceTreeRanges("/proc/device-tree/soc/ranges", 8);
    }
    address = (address == (uint32_t)~0) ? 0x20000000 : address;

    return address;
}
#elif defined __NetBSD__ && (!defined(__x86_64__) || defined(__X86__))
uint32_t SBC_Version::GetPeripheralAddress(void)
{
    char buf[1024];
    size_t len = sizeof(buf);
    uint32_t address;

    if (sysctlbyname("hw.model", buf, &len, NULL, 0) || strstr(buf, "ARM1176JZ-S") != buf)
    {
        // Failed to get CPU model || Not BCM2835
        // use the address of BCM283[67]
        address = 0x3f000000;
    }
    else
    {
        // Use BCM2835 address
        address = 0x20000000;
    }
    return address;
}
#else
uint32_t SBC_Version::GetPeripheralAddress(void)
{
    return 0;
}
#endif
