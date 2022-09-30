//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-22 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <list>
#include <sstream>
#include "rascsi_interface.pb.h"
#include "rasutil.h"

using namespace std;
using namespace rascsi_interface;

bool ras_util::GetAsInt(const string& value, int& result)
{
	if (value.find_first_not_of("0123456789") != string::npos) {
		return false;
	}

	try {
		auto v = stoul(value);
		result = (int)v;
	}
	catch(const invalid_argument&) {
		return false;
	}
	catch(const out_of_range&) {
		return false;
	}

	return true;
}

string ras_util::ListDevices(const list<PbDevice>& pb_devices)
{
	if (pb_devices.empty()) {
		return "No devices currently attached.";
	}

	ostringstream s;
	s << "+----+-----+------+-------------------------------------" << endl
			<< "| ID | LUN | TYPE | IMAGE FILE" << endl
			<< "+----+-----+------+-------------------------------------" << endl;

	list<PbDevice> devices = pb_devices;
	devices.sort([](const auto& a, const auto& b) { return a.id() < b.id() || a.unit() < b.unit(); });

	for (const auto& device : devices) {
		string filename;
		switch (device.type()) {
			case SCBR:
				filename = "X68000 HOST BRIDGE";
				break;

			case SCDP:
				filename = "DaynaPort SCSI/Link";
				break;

			case SCHS:
				filename = "Host Services";
				break;

			case SCLP:
				filename = "SCSI Printer";
				break;

			default:
				filename = device.file().name();
				break;
		}

		s << "|  " << device.id() << " |   " << device.unit() << " | " << PbDeviceType_Name(device.type()) << " | "
				<< (filename.empty() ? "NO MEDIA" : filename)
				<< (!device.status().removed() && (device.properties().read_only() || device.status().protected_()) ? " (READ-ONLY)" : "")
				<< endl;
	}

	s << "+----+-----+------+-------------------------------------";

	return s.str();
}

// Pin the thread to a specific CPU
void ras_util::FixCpu(int cpu)
{
#ifdef __linux
	// Get the number of CPUs
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
	int cpus = CPU_COUNT(&cpuset);

	// Set the thread affinity
	if (cpu < cpus) {
		CPU_ZERO(&cpuset);
		CPU_SET(cpu, &cpuset);
		sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
	}
#endif
}
