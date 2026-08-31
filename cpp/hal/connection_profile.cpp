//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Powered by XM6 TypeG Technology.
// Copyright (C) 2016-2020 GIMONS
//
//---------------------------------------------------------------------------

#include "hal/connection_profile.h"

#include <algorithm>
#include <cctype>
#include <iterator>

using namespace std;

namespace
{
const connection_profile standard_profile = {.type        = connection_type::standard,
                                             .name        = "STANDARD",
                                             .description = "STANDARD",
                                             .pins        = {.act  = 4,
                                                             .enb  = 5,
                                                             .ind  = -1,
                                                             .tad  = -1,
                                                             .dtd  = -1,
                                                             .data = {10, 11, 12, 13, 14, 15, 16, 17, 18},
                                                             .atn  = 19,
                                                             .rst  = 20,
                                                             .ack  = 21,
                                                             .req  = 22,
                                                             .msg  = 23,
                                                             .cd   = 24,
                                                             .io   = 25,
                                                             .bsy  = 26,
                                                             .sel  = 27}};

const connection_profile fullspec_profile = {.type        = connection_type::fullspec,
                                             .name        = "FULLSPEC",
                                             .description = "FULLSPEC",
                                             .pins        = {.act  = 4,
                                                             .enb  = 5,
                                                             .ind  = 6,
                                                             .tad  = 7,
                                                             .dtd  = 8,
                                                             .data = {10, 11, 12, 13, 14, 15, 16, 17, 18},
                                                             .atn  = 19,
                                                             .rst  = 20,
                                                             .ack  = 21,
                                                             .req  = 22,
                                                             .msg  = 23,
                                                             .cd   = 24,
                                                             .io   = 25,
                                                             .bsy  = 26,
                                                             .sel  = 27}};

const connection_profile gamernium_profile = {.type        = connection_type::gamernium,
                                              .name        = "GAMERNIUM",
                                              .description = "GAMERnium.com version",
                                              .pins        = {.act  = 14,
                                                              .enb  = 6,
                                                              .ind  = 7,
                                                              .tad  = 8,
                                                              .dtd  = 5,
                                                              .data = {21, 26, 20, 19, 16, 13, 12, 11, 25},
                                                              .atn  = 10,
                                                              .rst  = 22,
                                                              .ack  = 24,
                                                              .req  = 15,
                                                              .msg  = 17,
                                                              .cd   = 18,
                                                              .io   = 4,
                                                              .bsy  = 27,
                                                              .sel  = 23}};

const array<const connection_profile*, 3> profiles = {&standard_profile, &fullspec_profile, &gamernium_profile};

string ToUpper(string_view value)
{
    string result(value);
    ranges::transform(result, result.begin(), [](unsigned char c) { return static_cast<char>(toupper(c)); });
    return result;
}
} // namespace

const connection_profile* selected_connection_profile = &fullspec_profile;

const connection_profile* FindConnectionProfile(string_view value)
{
    const string name = ToUpper(value);
    const auto it =
        ranges::find_if(profiles, [&name](const connection_profile* profile) { return profile->name == name; });
    return it == profiles.end() ? nullptr : *it;
}

bool SetConnectionType(string_view value, string& error)
{
    if (const auto* profile = FindConnectionProfile(value); profile != nullptr) {
        selected_connection_profile = profile;
        return true;
    }

    error = "Invalid connection type '" + string(value) + "'. Supported types are STANDARD, FULLSPEC, and GAMERNIUM";
    return false;
}

bool ConfigureConnectionType(vector<char*>& args, string& error)
{
    for (auto it = args.begin() + 1; it != args.end();) {
        const string_view option(*it);
        string_view value;

        if (option == "-C") {
            const auto value_it = next(it);
            if (value_it == args.end()) {
                error = "Missing value for -C";
                return false;
            }
            value = *value_it;
            it    = args.erase(it, next(value_it));
        } else {
            ++it;
            continue;
        }

        if (!SetConnectionType(value, error)) {
            return false;
        }
    }

    return true;
}
