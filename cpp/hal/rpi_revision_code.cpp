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

#include "rpi_revision_code.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fmt/core.h>
#include <iostream>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

Rpi_Revision_Code::Rpi_Revision_Code(const string &cpuinfo_path) {
  std::string cmd = "cat " + cpuinfo_path + "| awk '/Revision/ {print $3}'";

  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);

  if (!pipe) {
    spdlog::error(
        "Unable to parse the /proc/cpuinfo. Are you running as root?");
    SetValid(false);
  } else {
    spdlog::debug("Found cpuinfo. Parsing...");
    SetValid(true);
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe.get()) !=
           nullptr) {
      result += buffer.data();
    }

    // Remove trailing newline
    size_t nl_position = result.find('\n');
    if (nl_position != string::npos) {
      result.erase(nl_position);
    }

    if (result.length() > 8) {
      spdlog::warn("The revision code is too long. It may be truncated.");
      SetValid(false);
    }
    if (result.length() < 6) {
      spdlog::warn("The revision code is too short. It may be padded.");
      SetValid(false);
    }
    spdlog::debug("cpuinfo data: <" + result + ">");

    rpi_revcode = strtol(result.c_str(), NULL, 16);

    if (rpi_revcode == 0xFFFFFFFF) {
      spdlog::warn(
          "The revision code is invalid. This may be a hardware issue.");
      SetValid(false);
    }
    if (!valid_rpi_version_type(Type())) {
      spdlog::warn("Invalid CPU type detected {}. Please raise Github issue",
                   (uint32_t)Type());
      SetValid(false);
    }
    if (!valid_memory_size_type(MemorySize())) {
      spdlog::info("Invalid memory size detected {}. Please raise Github issue",
                   (uint8_t)MemorySize());
      // The amount of memory available doesn't matter to PiSCSI
    }
    if (!valid_manufacturer_type(Manufacturer())) {
      spdlog::info(
          "Invalid manufacturer type detected {}. Please raise Github issue",
          (uint8_t)Manufacturer());
      // The manufacturer doesn't matter to PiSCSI
    }
    if (!valid_cpu_type(Processor())) {
      spdlog::warn("Invalid CPU type detected {}. Please raise Github issue",
                   (uint8_t)Processor());
      SetValid(false);
    }
    if (!IsValid()) {
      string hex_rev_code = fmt::format("{:08X}", rpi_revcode);
      spdlog::error("Invalid cpuinfo detected!! Raspberry Pi Revision code is: {} result code: {}",
                   hex_rev_code.c_str(), result.c_str());
    }
    else{
      spdlog::info("Detected " + TypeStr() + " " + RevisionStr() + " " + MemorySizeStr() + " (" + ProcessorStr() + ")");
    }
  }
}

uint32_t Rpi_Revision_Code::extract_bits(int start_bit, int size) {
  unsigned mask = ((1 << size) - 1) << start_bit;
  return (rpi_revcode & mask) >> start_bit;
}

  std::string Rpi_Revision_Code::RevisionStr(){
    return "1." + std::to_string(Revision());
  }
  std::string Rpi_Revision_Code::TypeStr(){
    if (valid_rpi_version_type(Type())){
      return rpi_version_type_string[(uint32_t)Type()];
    }
    else{
      return "<<Invalid Rpi Version>>";
    }

  }
  std::string Rpi_Revision_Code::ProcessorStr(){
    if(valid_cpu_type(Processor())){
      return cpu_type_string[(int)Processor()];
    }else{
      return "<<Invalid CPU Type>>";
    }

  }
  std::string Rpi_Revision_Code::ManufacturerStr(){
    if(valid_manufacturer_type(Manufacturer())){
      return manufacturer_type_string[(int)Manufacturer()];
    }else{
      return "<<Invalid Manufacturer>>";
    }
  }
  std::string Rpi_Revision_Code::MemorySizeStr(){
    if(valid_memory_size_type(MemorySize())){
      return memory_size_type_string[(int)MemorySize()];
    }else{
      return "<<Invalid Memory Size>>";
    }
  }