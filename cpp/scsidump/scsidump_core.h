//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include <memory>
#include <string>
#include <span>
#include <vector>
#include <unordered_map>
#include <stdexcept>

using namespace std;

class phase_exception : public runtime_error
{
	using runtime_error::runtime_error;
};

class ScsiDump
{

  public:

    ScsiDump()  = default;
    ~ScsiDump() = default;

    int run(const span<char *>);

    struct inquiry_info {
        string vendor;
        string product;
        string revision;
        uint32_t sector_size;
        uint64_t capacity;

        void GeneratePropertiesFile(const string&) const;
    };
    using inquiry_info_t = struct inquiry_info;

  private:

    bool Banner(span<char *>) const;
    bool Init() const;
    void ParseArguments(span<char *>);
    void DisplayBoardId() const;
    void ScanBus();
    bool DisplayInquiry(inquiry_info_t&, bool);
    int DumpRestore();
    bool GetDeviceInfo(inquiry_info_t&);
    void WaitForPhase(phase_t) const;
    void Selection() const;
    void Command(scsi_defs::scsi_command, vector<uint8_t>&) const;
    void DataIn(int);
    void DataOut(int);
    void Status() const;
    void MessageIn() const;
    void BusFree() const;
    void TestUnitReady() const;
    void RequestSense();
    void Inquiry();
    pair<uint64_t, uint32_t> ReadCapacity();
    void Read10(uint32_t, uint32_t, uint32_t);
    void Write10(uint32_t, uint32_t, uint32_t);
    void WaitForBusy() const;

    static void CleanUp();
    static void TerminationHandler(int);

    // A static instance is needed because of the signal handler
    static inline unique_ptr<BUS> bus;

    vector<uint8_t> buffer;

    int target_id = -1;

    int target_lun = 0;

    int initiator_id = 7;

    string filename;

    uint64_t start_sector = 0;

    bool inquiry = false;

    bool scan_bus = false;

    bool restore = false;

    bool properties_file = false;

    static const int MINIMUM_BUFFER_SIZE = 1024 * 64;
    static const int DEFAULT_BUFFER_SIZE = 1024 * 1024;

    static inline const string DIVIDER = "----------------------------------------";

    static inline const unordered_map<byte, string> DEVICE_TYPES = {
    		{ byte{0}, "Direct Access" },
			{ byte{1}, "Sequential Access" },
			{ byte{2}, "Printer" },
			{ byte{3}, "Processor" },
			{ byte{4}, "Write-Once" },
			{ byte{5}, "CD-ROM/DVD/BD/DVD-RAM" },
			{ byte{6}, "Scanner" },
			{ byte{7}, "Optical Memory" },
			{ byte{8}, "Media Changer" },
			{ byte{9}, "Communications" },
			{ byte{10}, "Graphic Arts Pre-Press" },
			{ byte{11}, "Graphic Arts Pre-Press" },
			{ byte{12}, "Storage Array Controller" },
			{ byte{13}, "Enclosure Services" },
			{ byte{14}, "Simplified Direct Access" },
			{ byte{15}, "Optical Card Reader/Writer" },
			{ byte{16}, "Bridge Controller" },
			{ byte{17}, "Object-based Storage" },
			{ byte{18}, "Automation/Drive Interface" },
			{ byte{19}, "Security Manager" },
			{ byte{20}, "Host Managed Zoned Block" },
			{ byte{30}, "Well Known Logical Unit" }
    };
};
