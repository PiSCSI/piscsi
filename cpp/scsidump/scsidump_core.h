//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include "shared/scsi.h"
#include <memory>
#include <string>
#include <vector>

using namespace std;

class ScsiDump
{
    static const int MINIMUM_BUFFER_SIZE = 1024 * 64;
    static const int DEFAULT_BUFFER_SIZE = 1024 * 1024;

  public:
    ScsiDump()  = default;
    ~ScsiDump() = default;

    int run(const vector<char *> &);

    struct inquiry_info_struct {
        string vendor;
        string product;
        string revision;
        uint32_t sector_size;
        uint64_t capacity;
    };
	using inquiry_info_t = struct inquiry_info_struct;

  protected:
    // Protected for testability
    static void GeneratePropertiesFile(const string &filename, const inquiry_info_t &inq_info);

  private:
    bool Banner(const vector<char *> &) const;
    bool Init() const;
    void ParseArguments(const vector<char *> &);
    int DumpRestore();
    inquiry_info_t GetDeviceInfo();
    void WaitPhase(phase_t) const;
    void Selection() const;
    void Command(scsi_defs::scsi_command, vector<uint8_t> &) const;
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
    static void KillHandler(int);

    // A static instance is needed because of the signal handler
    static inline unique_ptr<BUS> bus;

    vector<uint8_t> buffer;

    int target_id = -1;

    int target_lun = 0;

    int initiator_id = 7;

    string filename;

    bool restore = false;

    const string divider_str = "----------------------------------------";
};
