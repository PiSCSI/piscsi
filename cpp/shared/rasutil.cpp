//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2021-2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "rascsi_version.h"
#include "rasutil.h"
#include <sstream>
#include <algorithm>

using namespace std;

bool ras_util::GetAsUnsignedInt(const string& value, int& result)
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

string ras_util::ProcessId(const string& id_spec, int max_luns, int& id, int& lun)
{
	if (const size_t separator_pos = id_spec.find(COMPONENT_SEPARATOR); separator_pos == string::npos) {
		if (!GetAsUnsignedInt(id_spec, id) || id >= 8) {
			return "Invalid device ID (0-7)";
		}

		lun = 0;
	}
	else if (!GetAsUnsignedInt(id_spec.substr(0, separator_pos), id) || id > 7 ||
			!GetAsUnsignedInt(id_spec.substr(separator_pos + 1), lun) || lun >= max_luns) {
		return "Invalid LUN (0-" + to_string(max_luns - 1) + ")";
	}

	return "";
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
