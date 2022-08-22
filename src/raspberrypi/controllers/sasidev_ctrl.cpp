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
	// Phase change
	if (ctrl.phase != BUS::command) {
		LOGTRACE("%s Command Phase", __PRETTY_FUNCTION__);

		// Phase Setting
		ctrl.phase = BUS::command;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(FALSE);

		// Data transfer is 6 bytes x 1 block
		ctrl.offset = 0;
		ctrl.length = 6;
		ctrl.blocks = 1;

		// If no byte can be received move to the status phase
		int count = ctrl.bus->CommandHandShake(ctrl.buffer);
		if (!count) {
			Error();
			return;
		}

		ctrl.length = GPIOBUS::GetCommandByteCount(ctrl.buffer[0]);

		// If not able to receive all, move to the status phase
		if (count != (int)ctrl.length) {
			Error();
			return;
		}

		// Command data transfer
		for (int i = 0; i < (int)ctrl.length; i++) {
			ctrl.cmd[i] = (DWORD)ctrl.buffer[i];
			LOGTRACE("%s CDB[%d]=$%02X",__PRETTY_FUNCTION__, i, ctrl.cmd[i]);
		}

		// Clear length and block
		ctrl.length = 0;
		ctrl.blocks = 0;

		// Execution Phase
		Execute();
	}
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
	// Phase change
	if (ctrl.phase != BUS::status) {
		// Minimum execution time
		if (ctrl.execstart > 0) {
			DWORD time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < min_exec_time_scsi) {
				SysTimer::SleepUsec(min_exec_time_scsi - time);
			}
			ctrl.execstart = 0;
		} else {
			SysTimer::SleepUsec(5);
		}

		LOGTRACE("%s Status phase", __PRETTY_FUNCTION__);

		// Phase Setting
		ctrl.phase = BUS::status;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(TRUE);

		// Data transfer is 1 byte x 1 block
		ctrl.offset = 0;
		ctrl.length = 1;
		ctrl.blocks = 1;
		ctrl.buffer[0] = (BYTE)ctrl.status;

		LOGTRACE( "%s Status Phase $%02X",__PRETTY_FUNCTION__, (unsigned int)ctrl.status);

		return;
	}

	// Send
	Send();
}

//---------------------------------------------------------------------------
//
//	Message in phase (used by SASI and SCSI)
//
//---------------------------------------------------------------------------
void SASIDEV::MsgIn()
{
	// Phase change
	if (ctrl.phase != BUS::msgin) {
		LOGTRACE("%s Starting Message in phase", __PRETTY_FUNCTION__);

		// Phase Setting
		ctrl.phase = BUS::msgin;

		// Signal line operated by the target
		ctrl.bus->SetMSG(TRUE);
		ctrl.bus->SetCD(TRUE);
		ctrl.bus->SetIO(TRUE);

		// length, blocks are already set
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.blocks > 0);
		ctrl.offset = 0;
		return;
	}

	//Send
	LOGTRACE("%s Transitioning to Send()", __PRETTY_FUNCTION__);
	Send();
}

//---------------------------------------------------------------------------
//
//	Data-in Phase (used by SASI and SCSI)
//
//---------------------------------------------------------------------------
void SASIDEV::DataIn()
{
	ASSERT(ctrl.length >= 0);

	// Phase change
	if (ctrl.phase != BUS::datain) {
		// Minimum execution time
		if (ctrl.execstart > 0) {
			DWORD time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < min_exec_time_scsi) {
				SysTimer::SleepUsec(min_exec_time_scsi - time);
			}
			ctrl.execstart = 0;
		}

		// If the length is 0, go to the status phase
		if (ctrl.length == 0) {
			Status();
			return;
		}

		LOGTRACE("%s Going into Data-in Phase", __PRETTY_FUNCTION__);
		// Phase Setting
		ctrl.phase = BUS::datain;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(TRUE);

		// length, blocks are already set
		ASSERT(ctrl.length > 0);
		ASSERT(ctrl.blocks > 0);
		ctrl.offset = 0;

		return;
	}

	// Send
	Send();
}

//---------------------------------------------------------------------------
//
//	Data out phase (used by SASI and SCSI)
//
//---------------------------------------------------------------------------
void SASIDEV::DataOut()
{
	ASSERT(ctrl.length >= 0);

	// Phase change
	if (ctrl.phase != BUS::dataout) {
		// Minimum execution time
		if (ctrl.execstart > 0) {
			DWORD time = SysTimer::GetTimerLow() - ctrl.execstart;
			if (time < min_exec_time_scsi) {
				SysTimer::SleepUsec(min_exec_time_scsi - time);
			}
			ctrl.execstart = 0;
		}

		// If the length is 0, go to the status phase
		if (ctrl.length == 0) {
			Status();
			return;
		}

		LOGTRACE("%s Data out phase", __PRETTY_FUNCTION__);

		// Phase Setting
		ctrl.phase = BUS::dataout;

		// Signal line operated by the target
		ctrl.bus->SetMSG(FALSE);
		ctrl.bus->SetCD(FALSE);
		ctrl.bus->SetIO(FALSE);

		// Length has already been calculated
		ASSERT(ctrl.length > 0);
		ctrl.offset = 0;
		return;
	}

	// Receive
	Receive();
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

//---------------------------------------------------------------------------
//
//	Data transfer IN
//	*Reset offset and length
// TODO XferIn probably needs a dispatcher, in order to avoid subclassing Disk, i.e.
// just like the actual SCSI commands XferIn should be executed by the respective device
//
//---------------------------------------------------------------------------
bool SASIDEV::XferIn(BYTE *buf)
{
	ASSERT(ctrl.phase == BUS::datain);
	LOGTRACE("%s ctrl.cmd[0]=%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl.cmd[0]);

	// Logical Unit
	DWORD lun = GetEffectiveLun();
	if (!ctrl.unit[lun]) {
		return false;
	}

	// Limited to read commands
	switch (ctrl.cmd[0]) {
		case eCmdRead6:
		case eCmdRead10:
		case eCmdRead16:
			// Read from disk
			ctrl.length = ((Disk *)ctrl.unit[lun])->Read(ctrl.cmd, buf, ctrl.next);
			ctrl.next++;

			// If there is an error, go to the status phase
			if (ctrl.length <= 0) {
				// Cancel data-in
				return false;
			}

			// If things are normal, work setting
			ctrl.offset = 0;
			break;

		// Other (impossible)
		default:
			ASSERT(FALSE);
			return false;
	}

	// Succeeded in setting the buffer
	return true;
}

//---------------------------------------------------------------------------
//
//	Data transfer OUT
//	*If cont=true, reset the offset and length
// TODO XferOut probably needs a dispatcher, in order to avoid subclassing Disk, i.e.
// just like the actual SCSI commands XferOut should be executed by the respective device
//
//---------------------------------------------------------------------------
bool SASIDEV::XferOut(bool cont)
{
	ASSERT(ctrl.phase == BUS::dataout);

	// Logical Unit
	DWORD lun = GetEffectiveLun();
	if (!ctrl.unit[lun]) {
		return false;
	}
	Disk *device = (Disk *)ctrl.unit[lun];

	switch (ctrl.cmd[0]) {
		case SASIDEV::eCmdModeSelect6:
		case SASIDEV::eCmdModeSelect10:
			if (!device->ModeSelect(ctrl.cmd, ctrl.buffer, ctrl.offset)) {
				// MODE SELECT failed
				return false;
			}
			break;

		case SASIDEV::eCmdWrite6:
		case SASIDEV::eCmdWrite10:
		case SASIDEV::eCmdWrite16:
		case SASIDEV::eCmdVerify10:
		case SASIDEV::eCmdVerify16:
		{
			// If we're a host bridge, use the host bridge's SendMessage10 function
			// TODO This class must not know about SCSIBR
			SCSIBR *bridge = dynamic_cast<SCSIBR *>(device);
			if (bridge) {
				if (!bridge->SendMessage10(ctrl.cmd, ctrl.buffer)) {
					// write failed
					return false;
				}

				// If normal, work setting
				ctrl.offset = 0;
				break;
			}

			// Special case Write function for DaynaPort
			// TODO This class must not know about DaynaPort
			SCSIDaynaPort *daynaport = dynamic_cast<SCSIDaynaPort *>(device);
			if (daynaport) {
				if (!daynaport->Write(ctrl.cmd, ctrl.buffer, ctrl.length)) {
					// write failed
					return false;
				}

				// If normal, work setting
				ctrl.offset = 0;
				ctrl.blocks = 0;
				break;
			}

			if (!device->Write(ctrl.cmd, ctrl.buffer, ctrl.next - 1)) {
				// Write failed
				return false;
			}

			// If you do not need the next block, end here
			ctrl.next++;
			if (!cont) {
				break;
			}

			// Check the next block
			ctrl.length = device->WriteCheck(ctrl.next - 1);
			if (ctrl.length <= 0) {
				// Cannot write
				return false;
			}

			// If normal, work setting
			ctrl.offset = 0;
			break;
		}

		case SASIDEV::eCmdSetMcastAddr:
			LOGTRACE("%s Done with DaynaPort Set Multicast Address", __PRETTY_FUNCTION__);
			break;

		default:
			LOGWARN("Received an unexpected command ($%02X) in %s", (WORD)ctrl.cmd[0] , __PRETTY_FUNCTION__)
			break;
	}

	// Buffer saved successfully
	return true;
}

//---------------------------------------------------------------------------
//
//	Logical unit flush
//
//---------------------------------------------------------------------------
void SASIDEV::FlushUnit()
{
	ASSERT(ctrl.phase == BUS::dataout);

	// Logical Unit
	DWORD lun = GetEffectiveLun();
	if (!ctrl.unit[lun]) {
		return;
	}

	Disk *disk = (Disk *)ctrl.unit[lun];

	// WRITE system only
	switch ((SASIDEV::sasi_command)ctrl.cmd[0]) {
		case SASIDEV::eCmdWrite6:
		case SASIDEV::eCmdWrite10:
		case SASIDEV::eCmdWrite16:
		case SASIDEV::eCmdWriteLong16:
		case SASIDEV::eCmdVerify10:
		case SASIDEV::eCmdVerify16:
			break;

		case SASIDEV::eCmdModeSelect6:
		case SASIDEV::eCmdModeSelect10:
            // Debug code related to Issue #2 on github, where we get an unhandled Mode Select when
            // the mac is rebooted
            // https://github.com/akuker/RASCSI/issues/2
            LOGWARN("Received \'Mode Select\'\n");
            LOGWARN("   Operation Code: [%02X]\n", (WORD)ctrl.cmd[0]);
            LOGWARN("   Logical Unit %01X, PF %01X, SP %01X [%02X]\n",\
			   (WORD)ctrl.cmd[1] >> 5, 1 & ((WORD)ctrl.cmd[1] >> 4), \
			   (WORD)ctrl.cmd[1] & 1, (WORD)ctrl.cmd[1]);
            LOGWARN("   Reserved: %02X\n", (WORD)ctrl.cmd[2]);
            LOGWARN("   Reserved: %02X\n", (WORD)ctrl.cmd[3]);
            LOGWARN("   Parameter List Len %02X\n", (WORD)ctrl.cmd[4]);
            LOGWARN("   Reserved: %02X\n",(WORD)ctrl.cmd[5]);
            LOGWARN("   Ctrl Len: %08X\n",(WORD)ctrl.length);

			if (!disk->ModeSelect(
				ctrl.cmd, ctrl.buffer, ctrl.offset)) {
				// MODE SELECT failed
				LOGWARN("Error occured while processing Mode Select command %02X\n", (unsigned char)ctrl.cmd[0]);
				return;
			}
            break;

		case SASIDEV::eCmdSetMcastAddr:
			// TODO: Eventually, we should store off the multicast address configuration data here...
			break;

		default:
			LOGWARN("Received an unexpected flush command $%02X\n",(WORD)ctrl.cmd[0]);
			break;
	}
}

int SASIDEV::GetEffectiveLun() const
{
	return (ctrl.cmd[1] >> 5) & 0x07;
}

