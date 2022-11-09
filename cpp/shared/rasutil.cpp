//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-22 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rascsi_version.h"
#include "rasutil.h"
#include <sstream>
#include <algorithm>

using namespace std;

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

string ras_util::Banner(const string& app)
{
	ostringstream s;

	s << "SCSI Target Emulator RaSCSI " << app << " version " << rascsi_get_version_string()
			<< "  (" << __DATE__ << ' ' << __TIME__ << ")\n";
	s << "Powered by XM6 TypeG Technology / ";
	s << "Copyright (C) 2016-2020 GIMONS\n";
	s << "Copyright (C) 2020-2022 Contributors to the RaSCSI Reloaded project\n";

	return s.str();
}

string ras_util::GetExtensionLowerCase(const string& filename)
{
	string ext;
	if (const size_t separator = filename.rfind('.'); separator != string::npos) {
		ext = filename.substr(separator + 1);
	}
	transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

	return ext;
}

// Pin the thread to a specific CPU
// TODO Check whether just using a single CPU really makes sense
void ras_util::FixCpu(int cpu)
{
#ifdef __linux__
	// Get the number of CPUs
	cpu_set_t mask;
	CPU_ZERO(&mask);
	sched_getaffinity(0, sizeof(cpu_set_t), &mask);
	const int cpus = CPU_COUNT(&mask);

	// Set the thread affinity
	if (cpu < cpus) {
		CPU_ZERO(&mask);
		CPU_SET(cpu, &mask);
		sched_setaffinity(0, sizeof(cpu_set_t), &mask);
	}
#endif
}
