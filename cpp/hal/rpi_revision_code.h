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

class Rpi_Revision_Code {
public:
  enum class memory_size_type : uint8_t {
    MEM_256MB = 0,
    MEM_512MB = 1,
    MEM_1GB = 2,
    MEM_2GB = 3,
    MEM_4GB = 4,
    MEM_8GB = 5,
    MEM_INVALID = 0x7,
  };
  bool valid_memory_size_type(memory_size_type value) {
    return (value >= memory_size_type::MEM_256MB &&
            value <= memory_size_type::MEM_8GB);
  }
  enum class manufacturer_type : uint8_t {
    SonyUK = 0,
    Egoman = 1,
    Embest2 = 2,
    SonyJapan = 3,
    Embest4 = 4,
    Stadium = 5,
    MANUFACTURER_INVALID = 0xF,
  };

  bool valid_manufacturer_type(manufacturer_type value) {
    return (value >= manufacturer_type::SonyUK &&
            value <= manufacturer_type::Stadium);
  }

  enum class cpu_type : uint8_t {
    BCM2835 = 0,
    BCM2836 = 1,
    BCM2837 = 2,
    BCM2711 = 3,
    BCM2712 = 4,
    CPU_TYPE_INVALID = 0xF,
  };
  bool valid_cpu_type(cpu_type value) {
    return (value >= cpu_type::BCM2835 && value <= cpu_type::BCM2712);
  }

  enum class rpi_version_type : uint8_t {
    rpi_version_A = 0,
    rpi_version_B = 1,
    rpi_version_Aplus = 2,
    rpi_version_Bplus = 3,
    rpi_version_2B = 4,
    rpi_version_Alpha = 5, // (early prototype)
    rpi_version_CM1 = 6,
    rpi_version_3B = 8,
    rpi_version_Zero = 9,
    rpi_version_CM3 = 0xa,
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
  bool valid_rpi_version_type(rpi_version_type value) {
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
  uint8_t Revision() {
    return (uint8_t)extract_bits(0, 4);
  } //: 4;         // (bits 0-3)
  rpi_version_type Type() {
    return (rpi_version_type)extract_bits(4, 8);
  } //: // (bits 4-11)
  cpu_type Processor() { return (cpu_type)extract_bits(12, 4); } // (bits 12-15)
  manufacturer_type Manufacturer() {
    return (manufacturer_type)extract_bits(16, 4);
  } // (bits 16-19)
  memory_size_type MemorySize() {
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

private:
  uint32_t rpi_revcode;
  bool is_valid = true;
};
