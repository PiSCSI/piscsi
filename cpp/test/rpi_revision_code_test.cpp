//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2024 akuker
//
//---------------------------------------------------------------------------

#include "hal/rpi_revision_code.h"
#include "mocks.h"
#include <cstdlib>
#include "test/test_shared.h"
#include <list>
#include <filesystem>

static string create_cpu_info(uint32_t revision)
{
    std::stringstream cpuinfo_stream;
    cpuinfo_stream << "Hardware    : BCM";
    cpuinfo_stream << GenerateRandomString(4);
    cpuinfo_stream << "\nRevision    : ";
    cpuinfo_stream << std::hex << revision;
    cpuinfo_stream << "\nSerial      : ";
    cpuinfo_stream << GenerateRandomString(32);
    cpuinfo_stream << "\n";
    // printf("CPU INFO:\n%s\n----", cpuinfo_stream.str().c_str());
    return cpuinfo_stream.str();
}

typedef struct
{
    uint32_t rev_code;
    uint8_t revision;
    Rpi_Revision_Code::rpi_version_type type;
    Rpi_Revision_Code::cpu_type processor;
    Rpi_Revision_Code::manufacturer_type manufacturer;
    Rpi_Revision_Code::memory_size_type memorysize;
    bool newstyle;
    bool waranty;
    bool otpread;
    bool otpprogram;
    bool overvoltage;
    bool isvalid;
} rpi_revision_test_case;

std::list<rpi_revision_test_case> test_cases =
    {

        {
            .rev_code = 0x900021,
            .revision = 1,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_Aplus,
            .processor = Rpi_Revision_Code::cpu_type::BCM2835,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0x900032,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_Bplus,
            .processor = Rpi_Revision_Code::cpu_type::BCM2835,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0x900092,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_Zero,
            .processor = Rpi_Revision_Code::cpu_type::BCM2835,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0x900093,
            .revision = 3,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_Zero,
            .processor = Rpi_Revision_Code::cpu_type::BCM2835,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },
        {
            .rev_code = 0x9000c1,
            .revision = 1,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_ZeroW,
            .processor = Rpi_Revision_Code::cpu_type::BCM2835,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0x9020e0,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_3Aplus,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0x9020e1,
            .revision = 1,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_3Aplus,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0x920092,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_Zero,
            .processor = Rpi_Revision_Code::cpu_type::BCM2835,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::Embest2,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0x920093,
            .revision = 3,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_Zero,
            .processor = Rpi_Revision_Code::cpu_type::BCM2835,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::Embest2,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },
        {
            .rev_code = 0x900061,
            .revision = 1,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_CM1,
            .processor = Rpi_Revision_Code::cpu_type::BCM2835,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa01040,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_2B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2836,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa01041,
            .revision = 1,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_2B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2836,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },
        {
            .rev_code = 0xa02082,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_3B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },
        {
            .rev_code = 0xa020a0,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_CM3,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },
        {
            .rev_code = 0xa020d3,
            .revision = 3,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_3Bplus,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },
        {
            .rev_code = 0xa020d4,
            .revision = 4,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_3Bplus,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa02042,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_2B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },
        {
            .rev_code = 0xa21041,
            .revision = 1,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_2B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2836,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::Embest2,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa22042,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_2B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::Embest2,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa22082,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_3B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::Embest2,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa220a0,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_CM3,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::Embest2,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa32082,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_3B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyJapan,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa52082,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_3B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::Stadium,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa22083,
            .revision = 3,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_3B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::Embest2,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa02100,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_CM3plus,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa03111,
            .revision = 1,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xb03111,

            .revision = 1,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_2GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xb03112,
            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_2GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },
        {
            .rev_code = 0xb03114,

            .revision = 4,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_2GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xb03115,

            .revision = 5,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_2GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xc03111,

            .revision = 1,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_4GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xc03112,

            .revision = 2,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_4GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xc03114,
            .revision = 4,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_4GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xc03115,

            .revision = 5,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_4GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xd03114,

            .revision = 4,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_8GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xd03115,

            .revision = 5,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_4B,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_8GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xc03130,

            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_400,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_4GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xa03140,

            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_CM4,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_1GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xb03140,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_CM4,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_2GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xc03140,

            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_CM4,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_4GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xd03140,

            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_CM4,
            .processor = Rpi_Revision_Code::cpu_type::BCM2711,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_8GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0x902120,

            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_Zero2W,
            .processor = Rpi_Revision_Code::cpu_type::BCM2837,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_512MB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            .rev_code = 0xc04170,

            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_5,
            .processor = Rpi_Revision_Code::cpu_type::BCM2712,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_4GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },
        {
            .rev_code = 0xd04170,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_5,
            .processor = Rpi_Revision_Code::cpu_type::BCM2712,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_8GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            // Test Overvoltage flag
            .rev_code = 0x80d04170,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_5,
            .processor = Rpi_Revision_Code::cpu_type::BCM2712,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_8GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = true,
            .isvalid = true,
        },

        {
            // Test OTP Program flag
            .rev_code = 0x40d04170,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_5,
            .processor = Rpi_Revision_Code::cpu_type::BCM2712,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_8GB,
            .newstyle = true,
            .waranty = false,
            .otpread = false,
            .otpprogram = true,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            // Test OTP Read flag
            .rev_code = 0x20d04170,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_5,
            .processor = Rpi_Revision_Code::cpu_type::BCM2712,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_8GB,
            .newstyle = true,
            .waranty = false,
            .otpread = true,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            // Test Waranty flag
            .rev_code = 0x02d04170,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_5,
            .processor = Rpi_Revision_Code::cpu_type::BCM2712,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_8GB,
            .newstyle = true,
            .waranty = true,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            // Test New Style
            .rev_code = 0x00504170,
            .revision = 0,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_5,
            .processor = Rpi_Revision_Code::cpu_type::BCM2712,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::SonyUK,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_8GB,
            .newstyle = false,
            .waranty = false,
            .otpread = false,
            .otpprogram = false,
            .overvoltage = false,
            .isvalid = true,
        },

        {
            // Invalid Test
            .rev_code = 0xFFFFFFFF,
            .revision = 0xF,
            .type = Rpi_Revision_Code::rpi_version_type::rpi_version_invalid,
            .processor = Rpi_Revision_Code::cpu_type::CPU_TYPE_INVALID,
            .manufacturer = Rpi_Revision_Code::manufacturer_type::MANUFACTURER_INVALID,
            .memorysize = Rpi_Revision_Code::memory_size_type::MEM_INVALID,
            .newstyle = true,
            .waranty = true,
            .otpread = true,
            .otpprogram = true,
            .overvoltage = true,
            .isvalid = false,
        },
};

TEST(RpiRevisionCode, Initialization)
{
    const string cpuinfo_file = "/proc/cpuinfo";

    for (rpi_revision_test_case tc : test_cases)
    {
        // debug
        // printf("Test case: %x\n", tc.rev_code);
        string cpuinfo_str = create_cpu_info(tc.rev_code);
        path cpuinfo_path = CreateTempFileWithString(cpuinfo_file, cpuinfo_str);
        unique_ptr<Rpi_Revision_Code> rpi_info(new Rpi_Revision_Code(cpuinfo_path.string()));
        EXPECT_EQ(rpi_info->Revision(), tc.revision);
        EXPECT_EQ(rpi_info->Type(), tc.type);
        EXPECT_EQ(rpi_info->Processor(), tc.processor);
        EXPECT_EQ(rpi_info->Manufacturer(), tc.manufacturer);
        EXPECT_EQ(rpi_info->MemorySize(), tc.memorysize);
        EXPECT_EQ(rpi_info->NewStyle(), tc.newstyle);
        EXPECT_EQ(rpi_info->Waranty(), tc.waranty);
        EXPECT_EQ(rpi_info->OtpRead(), tc.otpread);
        EXPECT_EQ(rpi_info->OtpProgram(), tc.otpprogram);
        EXPECT_EQ(rpi_info->Overvoltage(), tc.overvoltage);
        EXPECT_EQ(rpi_info->IsValid(), tc.isvalid);
        DeleteTempFile(cpuinfo_file);
    }
}
