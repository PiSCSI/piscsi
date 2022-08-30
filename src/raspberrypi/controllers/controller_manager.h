//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
// Keeps track of and manages the controllers
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>
#include <unordered_map>

using namespace std;

class BUS;
class AbstractController;
class ScsiController;

class ControllerManager
{
	ControllerManager() {}
	~ControllerManager() {}

public:

	static ControllerManager& instance();

	ScsiController *CreateScsiController(BUS *, int);
	const vector<AbstractController *> FindAll();
	AbstractController *FindController(int);
	void DeleteAll();
	void ResetAll();
	void ClearAllLuns();

	static unordered_map<int, AbstractController *> controllers;
};
