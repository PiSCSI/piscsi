//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "piscsi_version.h"
#include "piscsi_util.h"
#include <cassert>
#include <sstream>
#include <algorithm>

using namespace std;

bool piscsi_util::GetAsUnsignedInt(const string& value, int& result)
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

string piscsi_util::ProcessId(const string& id_spec, int max_luns, int& id, int& lun)
{
	assert(max_luns > 0);

	id = -1;
	lun = -1;

	if (id_spec.empty()) {
		return "Missing device ID";
	}

	if (const size_t separator_pos = id_spec.find(COMPONENT_SEPARATOR); separator_pos == string::npos) {
		if (!GetAsUnsignedInt(id_spec, id) || id >= 8) {
			id = -1;

			return "Invalid device ID (0-7)";
		}

		lun = 0;
	}
	else if (!GetAsUnsignedInt(id_spec.substr(0, separator_pos), id) || id > 7 ||
			!GetAsUnsignedInt(id_spec.substr(separator_pos + 1), lun) || lun >= max_luns) {
		id = -1;
		lun = -1;

		return "Invalid LUN (0-" + to_string(max_luns - 1) + ")";
	}

	return "";
}

string piscsi_util::Banner(const string& app)
{
	ostringstream s;

	s << "SCSI Target Emulator PiSCSI " << app << "\n";
	s << "Version " << piscsi_get_version_string() << "  (" << __DATE__ << ' ' << __TIME__ << ")\n";
	s << "Powered by XM6 TypeG Technology / ";
	s << "Copyright (C) 2016-2020 GIMONS\n";
	s << "Copyright (C) 2020-2023 Contributors to the PiSCSI project\n";

	return s.str();
}

string piscsi_util::GetExtensionLowerCase(const string& filename)
{
	string ext;
	if (const size_t separator = filename.rfind('.'); separator != string::npos) {
		ext = filename.substr(separator + 1);
	}
	ranges::transform(ext, ext.begin(), [](unsigned char c){ return std::tolower(c); });

	return ext;
}

// Pin the thread to a specific CPU
// TODO Check whether just using a single CPU really makes sense
void piscsi_util::FixCpu(int cpu)
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
