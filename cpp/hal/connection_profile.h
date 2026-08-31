//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

enum class connection_type {
    standard,
    fullspec,
    gamernium,
};

struct connection_pins {
    int act;
    int enb;
    int ind;
    int tad;
    int dtd;
    std::array<int, 9> data;
    int atn;
    int rst;
    int ack;
    int req;
    int msg;
    int cd;
    int io;
    int bsy;
    int sel;
};

struct connection_profile {
    connection_type type;
    std::string_view name;
    std::string_view description;
    connection_pins pins;
};

extern const connection_profile* selected_connection_profile;

inline const connection_profile& GetConnectionProfile()
{
    return *selected_connection_profile;
}

const connection_profile* FindConnectionProfile(std::string_view);
bool SetConnectionType(std::string_view, std::string&);
bool ConfigureConnectionType(std::vector<char*>&, std::string&);

// These aliases keep GPIO code focused on SCSI signals while the selected
// profile supplies their BCM GPIO numbers at process startup.
#define CONNECT_DESC (GetConnectionProfile().description)
#define PIN_ACT (GetConnectionProfile().pins.act)
#define PIN_ENB (GetConnectionProfile().pins.enb)
#define PIN_IND (GetConnectionProfile().pins.ind)
#define PIN_TAD (GetConnectionProfile().pins.tad)
#define PIN_DTD (GetConnectionProfile().pins.dtd)
#define PIN_DT0 (GetConnectionProfile().pins.data[0])
#define PIN_DT1 (GetConnectionProfile().pins.data[1])
#define PIN_DT2 (GetConnectionProfile().pins.data[2])
#define PIN_DT3 (GetConnectionProfile().pins.data[3])
#define PIN_DT4 (GetConnectionProfile().pins.data[4])
#define PIN_DT5 (GetConnectionProfile().pins.data[5])
#define PIN_DT6 (GetConnectionProfile().pins.data[6])
#define PIN_DT7 (GetConnectionProfile().pins.data[7])
#define PIN_DP (GetConnectionProfile().pins.data[8])
#define PIN_ATN (GetConnectionProfile().pins.atn)
#define PIN_RST (GetConnectionProfile().pins.rst)
#define PIN_ACK (GetConnectionProfile().pins.ack)
#define PIN_REQ (GetConnectionProfile().pins.req)
#define PIN_MSG (GetConnectionProfile().pins.msg)
#define PIN_CD (GetConnectionProfile().pins.cd)
#define PIN_IO (GetConnectionProfile().pins.io)
#define PIN_BSY (GetConnectionProfile().pins.bsy)
#define PIN_SEL (GetConnectionProfile().pins.sel)

#define ACT_ON true
#define ENB_ON true
#define IND_IN false
#define TAD_IN false
#define DTD_IN true
