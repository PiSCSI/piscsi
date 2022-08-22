//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) akuker
//
//	Licensed under the BSD 3-Clause License. 
//	 See LICENSE file in the project root folder.
//
//	[ SASI device controller ]
//
//---------------------------------------------------------------------------
#include "controllers/sasidev_ctrl.h"
#include "filepath.h"
#include "gpiobus.h"
#include "devices/scsi_host_bridge.h"
#include "devices/scsi_daynaport.h"
#include <sstream>

//===========================================================================
//
//	SASI Device
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SASIDEV::SASIDEV()
{
	// Work initialization
	ctrl.phase = BUS::busfree;
	ctrl.m_scsi_id = UNKNOWN_SCSI_ID;
	ctrl.bus = NULL;
	memset(ctrl.cmd, 0x00, sizeof(ctrl.cmd));
	ctrl.status = 0x00;
	ctrl.message = 0x00;
	ctrl.execstart = 0;
	// The initial buffer size will default to either the default buffer size OR 
	// the size of an Ethernet message, whichever is larger.
	ctrl.bufsize = std::max(DEFAULT_BUFFER_SIZE, ETH_FRAME_LEN + 16 + ETH_FCS_LEN);
	ctrl.buffer = (BYTE *)malloc(ctrl.bufsize);
	memset(ctrl.buffer, 0x00, ctrl.bufsize);
	ctrl.blocks = 0;
	ctrl.next = 0;
	ctrl.offset = 0;
	ctrl.length = 0;
	ctrl.lun = -1;

	// Logical unit initialization
	for (int i = 0; i < UnitMax; i++) {
		ctrl.unit[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SASIDEV::~SASIDEV()
{
	// Free the buffer
	if (ctrl.buffer) {
		free(ctrl.buffer);
		ctrl.buffer = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Device reset
//
//---------------------------------------------------------------------------
void SASIDEV::Reset()
{
	// Work initialization
	memset(ctrl.cmd, 0x00, sizeof(ctrl.cmd));
	ctrl.phase = BUS::busfree;
	ctrl.status = 0x00;
	ctrl.message = 0x00;
	ctrl.execstart = 0;
	memset(ctrl.buffer, 0x00, ctrl.bufsize);
	ctrl.blocks = 0;
	ctrl.next = 0;
	ctrl.offset = 0;
	ctrl.length = 0;

	// Unit initialization
	for (int i = 0; i < UnitMax; i++) {
		if (ctrl.unit[i]) {
			ctrl.unit[i]->Reset();
		}
	}
}

//---------------------------------------------------------------------------
//
//	Connect the controller
//
//---------------------------------------------------------------------------
void SASIDEV::Connect(int id, BUS *bus)
{
	ctrl.m_scsi_id = id;
	ctrl.bus = bus;
}

//---------------------------------------------------------------------------
//
//	Set the logical unit
//
//---------------------------------------------------------------------------
void SASIDEV::SetUnit(int no, PrimaryDevice *dev)
{
	ASSERT(no < UnitMax);

	ctrl.unit[no] = dev;
}

//---------------------------------------------------------------------------
//
//	Check to see if this has a valid LUN
//
//---------------------------------------------------------------------------
bool SASIDEV::HasUnit()
{
	for (int i = 0; i < UnitMax; i++) {
		if (ctrl.unit[i]) {
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------------------------
//
//	Run
//
//---------------------------------------------------------------------------
BUS::phase_t SASIDEV::Process(int)
{
	assert(false);
	return ctrl.phase;
}

//---------------------------------------------------------------------------
//
//	Bus free phase
//
//---------------------------------------------------------------------------
void SASIDEV::BusFree()
{
	assert(false);
}

//---------------------------------------------------------------------------
//
//	Selection phase
//
//---------------------------------------------------------------------------
void SASIDEV::Selection()
{
	assert(false);
}

//---------------------------------------------------------------------------
//
//	Command phase (used by SASI and SCSI)
//
//---------------------------------------------------------------------------
void SASIDEV::Command()
{
	assert(false);
}

//---------------------------------------------------------------------------
//
//	Execution Phase
//
//---------------------------------------------------------------------------
void SASIDEV::Execute()
{
	assert(false);
}

//---------------------------------------------------------------------------
//
//	Status phase
//
//---------------------------------------------------------------------------
void SASIDEV::Status()
{
	assert(false);
}

//---------------------------------------------------------------------------
//
//	Message in phase (used by SASI and SCSI)
//
//---------------------------------------------------------------------------
void SASIDEV::MsgIn()
{
	assert(false);
}

//---------------------------------------------------------------------------
//
//	Data-in Phase (used by SASI and SCSI)
//
//---------------------------------------------------------------------------
void SASIDEV::DataIn()
{
	assert(false);
}

//---------------------------------------------------------------------------
//
//	Data out phase (used by SASI and SCSI)
//
//---------------------------------------------------------------------------
void SASIDEV::DataOut()
{
	assert(false);
}

void SASIDEV::Error(sense_key sense_key, asc asc, status status)
{
	assert(false);
}

//===========================================================================
//
//	Data transfer
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Data transmission
//
//---------------------------------------------------------------------------
void SASIDEV::Send()
{
	assert(false);
}

//---------------------------------------------------------------------------
//
//	Receive data
//
//---------------------------------------------------------------------------
void SASIDEV::Receive()
{
	assert(false);
}

int SASIDEV::GetEffectiveLun() const
{
	return (ctrl.cmd[1] >> 5) & 0x07;
}

