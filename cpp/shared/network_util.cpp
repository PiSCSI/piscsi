//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "network_util.h"
#ifdef __linux__
#include <cstring>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>
#endif

using namespace std;

vector<string> network_util::GetNetworkInterfaces()
{
	vector<string> network_interfaces;

#ifdef __linux__
	ifaddrs *addrs;
	getifaddrs(&addrs);
	ifaddrs *tmp = addrs;

	while (tmp) {
    	if (string name = tmp->ifa_name; tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET &&
    			name != "lo" && name != "piscsi_bridge" && name.rfind("dummy", 0) == string::npos) {
    		ifreq ifr = {};
    		strcpy(ifr.ifr_name, tmp->ifa_name); //NOSONAR Using strcpy is safe here

    		const int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    		// Only list interfaces that are up
    		if (!ioctl(fd, SIOCGIFFLAGS, &ifr) && (ifr.ifr_flags & IFF_UP)) {
    			network_interfaces.emplace_back(name);
	    	}

    		close(fd);
	    }

	    tmp = tmp->ifa_next;
	}

	freeifaddrs(addrs);
#endif

	return network_interfaces;
}
