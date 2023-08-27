//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "network_util.h"
#include <cstring>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <unistd.h>

using namespace std;

vector<string> network_util::GetNetworkInterfaces()
{
	vector<string> network_interfaces;

#ifdef __linux__
	ifaddrs *addrs;
	getifaddrs(&addrs);
	ifaddrs *tmp = addrs;

	while (tmp) {
	    if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_PACKET &&
	    		strcmp(tmp->ifa_name, "lo") && strcmp(tmp->ifa_name, "piscsi_bridge")) {
	        const int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

	        ifreq ifr = {};
	        strcpy(ifr.ifr_name, tmp->ifa_name); //NOSONAR Using strcpy is safe here
	        // Only list interfaces that are up
	        if (!ioctl(fd, SIOCGIFFLAGS, &ifr) && (ifr.ifr_flags & IFF_UP)) {
	        	network_interfaces.emplace_back(tmp->ifa_name);
	        }

	        close(fd);
	    }

	    tmp = tmp->ifa_next;
	}

	freeifaddrs(addrs);
#endif

	return network_interfaces;
}
