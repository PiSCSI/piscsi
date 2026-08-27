//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
// Copyright (C) 2026 Daniel Markstedt <daniel@mindani.net>
//
//---------------------------------------------------------------------------

#pragma once

#include "hal/bus.h"
#include <memory>
#include <span>
#include <string>
#include <vector>

using namespace std;

class SasiDump
{
  public:
    int run(span<char*>);

  private:
    static constexpr size_t BUFFER_SIZE         = 64 * 1024;
    static constexpr uint32_t MAX_BLOCK_ADDRESS = 0x1fffff;

    bool Banner(span<char*>) const;
    void ParseArguments(span<char*>);
    bool Init() const;
    void DumpRestore();
    void ResetBus() const;
    void SelectTarget() const;
    void SendCommand(vector<uint8_t>&) const;
    void Read6(uint32_t, uint32_t);
    void Write6(uint32_t, uint32_t);
    void TestUnitReady();
    void RequestSense();
    void ReadData(int);
    void WriteData(int);
    uint8_t ReceiveStatus() const;
    void ReceiveMessageIn() const;
    void WaitForPhase(phase_t) const;
    void WaitForBusy() const;
    void BusFree() const;

    static void CleanUp();
    static void TerminationHandler(int);

    static inline unique_ptr<BUS> bus;

    vector<uint8_t> buffer = vector<uint8_t>(BUFFER_SIZE);
    int target_id          = -1;
    int unit_id            = 0;
    uint32_t block_size    = 256;
    uint32_t block_count   = 0;
    string filename;
    bool restore = false;
};
