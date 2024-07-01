//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
//	for Raspberry Pi
//
//  Copyright (C) 2024 akuker
//
//	[ Detect Raspberry Pi version information ]
//
//  The Raspberry Pi version detection is based upon the example code
//  provided by the Raspberry Pi foundation:
//     https://github.com/raspberrypi/documentation/blob/develop/documentation/asciidoc/computers/raspberry-pi/revision-codes.adoc
//
//---------------------------------------------------------------------------

#pragma once
#include <cstdint>
#include <stdio.h>
#include <string>
#include <type_traits>
#include <vector>

using namespace std;

//===========================================================================
//
//	Raspberry Pi Revision Information
//
// With the launch of the Raspberry Pi 2, new-style revision codes were
// introduced. Rather than being sequential, each bit of the hex code
// represents a piece of information about the revision.
//
//===========================================================================

class Rpi_Revision_Code
{
public:
  enum class memory_size_type : uint8_t
  {
    MEM_256MB = 0,
    MEM_512MB = 1,
    MEM_1GB = 2,
    MEM_2GB = 3,
    MEM_4GB = 4,
    MEM_8GB = 5,
    MEM_INVALID = 0x7,
  };

private:
  const vector<std::string> memory_size_type_string = {
      "MEM_256MB",
      "MEM_512MB",
      "MEM_1GB",
      "MEM_2GB",
      "MEM_4GB",
      "MEM_8GB",
  };
  bool valid_memory_size_type(memory_size_type value)
  {
    return (value >= memory_size_type::MEM_256MB &&
            value <= memory_size_type::MEM_8GB);
  }

public:
  enum class manufacturer_type : uint8_t
  {
    SonyUK = 0,
    Egoman = 1,
    Embest2 = 2,
    SonyJapan = 3,
    Embest4 = 4,
    Stadium = 5,
    MANUFACTURER_INVALID = 0xF,
  };

private:
  const vector<std::string> manufacturer_type_string = {
      "SonyUK", "Egoman", "Embest2", "SonyJapan", "Embest4", "Stadium"};

  bool valid_manufacturer_type(manufacturer_type value)
  {
    return (value >= manufacturer_type::SonyUK &&
            value <= manufacturer_type::Stadium);
  }

public:
  enum class cpu_type : uint8_t
  {
    BCM2835 = 0,
    BCM2836 = 1,
    BCM2837 = 2,
    BCM2711 = 3,
    BCM2712 = 4,
    CPU_TYPE_INVALID = 0xF,
  };

private:
  const vector<std::string> cpu_type_string = {
      "BCM2835", "BCM2836", "BCM2837", "BCM2711", "BCM2712"};
  bool valid_cpu_type(cpu_type value)
  {
    return (value >= cpu_type::BCM2835 && value <= cpu_type::BCM2712);
  }

public:
  enum class rpi_version_type : uint8_t
  {
    rpi_version_A = 0x0,
    rpi_version_B = 0x1,
    rpi_version_Aplus = 0x2,
    rpi_version_Bplus = 0x3,
    rpi_version_2B = 0x4,
    rpi_version_Alpha = 0x5, // (early prototype)
    rpi_version_CM1 = 0x6,
    unused_7 = 0x7,
    rpi_version_3B = 0x8,
    rpi_version_Zero = 0x9,
    rpi_version_CM3 = 0xa,
    unused_B = 0xb,
    rpi_version_ZeroW = 0xc,
    rpi_version_3Bplus = 0xd,
    rpi_version_3Aplus = 0xe,
    rpi_version_InternalUseOnly1 = 0xf,
    rpi_version_CM3plus = 0x10,
    rpi_version_4B = 0x11,
    rpi_version_Zero2W = 0x12,
    rpi_version_400 = 0x13,
    rpi_version_CM4 = 0x14,
    rpi_version_CM4S = 0x15,
    rpi_version_InternalUseOnly2 = 0x16,
    rpi_version_5 = 0x17,
    rpi_version_invalid = 0xFF
  };

private:
  const vector<std::string> rpi_version_type_string = {
      "rpi_version_A",
      "rpi_version_B",
      "rpi_version_Aplus",
      "rpi_version_Bplus",
      "rpi_version_2B",
      "rpi_version_Alpha",
      "rpi_version_CM1",
      "unused_7",
      "rpi_version_3B",
      "rpi_version_Zero",
      "rpi_version_CM3",
      "unused_B",
      "rpi_version_ZeroW",
      "rpi_version_3Bplus",
      "rpi_version_3Aplus",
      "rpi_version_InternalUseOnly1",
      "rpi_version_CM3plus",
      "rpi_version_4B",
      "rpi_version_Zero2W",
      "rpi_version_400",
      "rpi_version_CM4",
      "rpi_version_CM4S",
      "rpi_version_InternalUseOnly2",
      "rpi_version_5",
      "unknown_24",
      "unknown_25",

  };
  bool valid_rpi_version_type(rpi_version_type value)
  {
    return (value >= rpi_version_type::rpi_version_A &&
            value <= rpi_version_type::rpi_version_5);
  }

public:
  Rpi_Revision_Code(const std::string &cpuinfo_path = "/proc/cpuinfo");
  Rpi_Revision_Code(uint32_t value) : rpi_revcode(value){};
  ~Rpi_Revision_Code() = default;

private:
  uint32_t extract_bits(int start_bit, int size);

public:
  uint8_t Revision()
  {
    return (uint8_t)extract_bits(0, 4);
  } //: 4;         // (bits 0-3)
  rpi_version_type Type()
  {
    return (rpi_version_type)extract_bits(4, 8);
  } //: // (bits 4-11)
  cpu_type Processor() { return (cpu_type)extract_bits(12, 4); } // (bits 12-15)
  manufacturer_type Manufacturer()
  {
    return (manufacturer_type)extract_bits(16, 4);
  } // (bits 16-19)
  memory_size_type MemorySize()
  {
    return (memory_size_type)extract_bits(20, 3);
  } // (bits 20-22)
  // 1: new-style revision
  // 0: old-style revision
  bool NewStyle() { return (bool)extract_bits(23, 1); } // (bit 23)
  // 0: Warranty is intact
  // 1: Warranty has been voided by overclocking
  bool Waranty() { return (bool)extract_bits(25, 1); } // (bit 25)
  // 0: OTP reading allowed
  // 1: OTP reading disallowed
  bool OtpRead() { return (bool)extract_bits(29, 1); } // (bit 29)
  // 0: OTP programming allowed
  // 1: OTP programming disallowed
  bool OtpProgram() { return (bool)extract_bits(30, 1); } // (bit 30)
  // 0: Overvoltage allowed
  // 1: Overvoltage disallowed
  bool Overvoltage() { return (bool)extract_bits(31, 3); } // (bit 31)

  bool IsValid() { return is_valid; }
  void SetValid(bool val) { is_valid = val; }

  std::string RevisionStr();
  std::string TypeStr();
  std::string ProcessorStr();
  std::string ManufacturerStr();
  std::string MemorySizeStr();

private:
  uint32_t rpi_revcode;
  bool is_valid = true;
};
