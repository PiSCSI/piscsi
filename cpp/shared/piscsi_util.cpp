//---------------------------------------------------------------------------
//
// SCSI Target Emulator PiSCSI
// for Raspberry Pi
//
// Copyright (C) 2021-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "controllers/controller_manager.h"
#include "piscsi_version.h"
#include "piscsi_util.h"
#include <spdlog/spdlog.h>
#include <cassert>
#include <cstring>
#include <sstream>
#include <filesystem>
#include <algorithm>

using namespace std;
using namespace filesystem;

vector<string> piscsi_util::Split(const string& s, char separator, int limit)
{
	assert(limit >= 0);

	string component;
	vector<string> result;
	stringstream str(s);

	while (--limit > 0 && getline(str, component, separator)) {
		result.push_back(component);
	}

	if (!str.eof()) {
		getline(str, component);
		result.push_back(component);
	}

	return result;
}

string piscsi_util::GetLocale()
{
	const char *locale = setlocale(LC_MESSAGES, "");
	if (locale == nullptr || !strcmp(locale, "C")) {
		locale = "en";
	}

	return locale;
}

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

string piscsi_util::ProcessId(const string& id_spec, int& id, int& lun)
{
	id = -1;
	lun = -1;

	if (id_spec.empty()) {
		return "Missing device ID";
	}

	const int id_max = ControllerManager::GetScsiIdMax();
	const int lun_max = ControllerManager::GetScsiLunMax();

	if (const auto& components = Split(id_spec, COMPONENT_SEPARATOR, 2); !components.empty()) {
		if (components.size() == 1) {
			if (!GetAsUnsignedInt(components[0], id) || id >= id_max) {
				id = -1;

				return "Invalid device ID (0-" + to_string(ControllerManager::GetScsiIdMax() - 1) + ")";
			}

			return "";
		}

		if (!GetAsUnsignedInt(components[0], id) || id >= id_max || !GetAsUnsignedInt(components[1], lun) || lun >= lun_max) {
			id = -1;
			lun = -1;

			return "Invalid LUN (0-" + to_string(lun_max - 1) + ")";
		}
	}

	return "";
}

string piscsi_util::Banner(string_view app)
{
	ostringstream s;

	s << "SCSI Target Emulator PiSCSI " << app << "\n";
	s << "Version " << piscsi_get_version_string() << "  (" << __DATE__ << ' ' << __TIME__ << ")\n";
	s << "Powered by XM6 TypeG Technology / ";
	s << "Copyright (C) 2016-2020 GIMONS\n";
	s << "Copyright (C) 2020-2024 Contributors to the PiSCSI project\n";

	return s.str();
}

string piscsi_util::GetExtensionLowerCase(string_view filename)
{
	string ext;
	ranges::transform(path(filename).extension().string(), back_inserter(ext), ::tolower);

	// Remove the leading dot
	return ext.empty() ? "" : ext.substr(1);
}

void piscsi_util::LogErrno(const string& msg)
{
	spdlog::error(errno ? msg + ": " + string(strerror(errno)) : msg);
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
