//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <set>
#include <map>
#include <optional>
#include <filesystem>
#include <cstdint>
#include <vector>

using namespace std;

struct sockaddr_in;

namespace network_util
{
	enum class NetworkInterfaceType {
		UNKNOWN,
		LOOPBACK,
		BRIDGE,
		ETHERNET,
		WIFI,
		TAP,
		VIRTUAL,
	};

	struct NetworkInterfaceInfo {
		string name;
		NetworkInterfaceType type = NetworkInterfaceType::UNKNOWN;
		bool up = false;
	};

	using network_interface_map = map<string, NetworkInterfaceInfo, less<>>;

	bool IsInterfaceUp(const string&);
	bool IsValidInterfaceName(const string&);
	vector<uint8_t> GetMacAddress(const string&);
	optional<string> ParseProxyArpUplink(const string&);
	optional<string> ReadProxyArpUplinkFile(const filesystem::path&);
	optional<string> GetConfiguredProxyArpUplink();
	NetworkInterfaceType ClassifyInterface(bool, bool, bool, bool, const string&);
	network_interface_map GetNetworkInterfaceInfo();
	set<string, less<>> GetNetworkInterfaces();
	bool ResolveHostName(const string&, sockaddr_in *);
}
