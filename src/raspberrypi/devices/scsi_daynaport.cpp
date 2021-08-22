//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI (*^..^*)
//	for Raspberry Pi
//
//  Copyright (C) 2020 akuker
//	Copyright (C) 2014-2020 GIMONS
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//
//  Licensed under the BSD 3-Clause License. 
//  See LICENSE file in the project root folder.
//
//  [ Emulation of the DaynaPort SCSI Link Ethernet interface ]
//
//  This design is derived from the SLINKCMD.TXT file, as well as David Kuder's 
//  Tiny SCSI Emulator
//    - SLINKCMD: http://www.bitsavers.org/pdf/apple/scsi/dayna/daynaPORT/SLINKCMD.TXT
//    - Tiny SCSI : https://hackaday.io/project/18974-tiny-scsi-emulator
//
//  Additional documentation and clarification is available at the 
//  following link:
//    - https://github.com/akuker/RASCSI/wiki/Dayna-Port-SCSI-Link
//
//  This does NOT include the file system functionality that is present
//  in the Sharp X68000 host bridge.
//
//  Note: This requires the DaynaPort SCSI Link driver.
//---------------------------------------------------------------------------

#include "scsi_daynaport.h"
#include <sstream>

//===========================================================================
//
//	DaynaPort SCSI Link Ethernet Adapter
//
//===========================================================================
const char* SCSIDaynaPort::m_vendor_name = "DAYNA   ";
const char* SCSIDaynaPort::m_device_name = "SCSI/Link       ";
const char* SCSIDaynaPort::m_revision = "1.4a";
const char* SCSIDaynaPort::m_firmware_version = "01.00.00";

const BYTE SCSIDaynaPort::m_bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const BYTE SCSIDaynaPort::m_apple_talk_addr[6] = { 0x09, 0x00, 0x07, 0xff, 0xff, 0xff };

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIDaynaPort::SCSIDaynaPort() : Disk("SCDP")
{
	SetRemovable(false);

	SetVendor("Dayna");
	SetProduct("SCSI/Link");

	#ifdef __linux__
	// TAP Driver Generation
	m_tap = new CTapDriver();
	m_bTapEnable = m_tap->Init();
	if(!m_bTapEnable){
		LOGERROR("Unable to open the TAP interface");
	}else {
		LOGDEBUG("Tap interface created");
	}

	LOGTRACE("%s this->reset()", __PRETTY_FUNCTION__);
	this->Reset();
	SetReady(true);
	SetReset(false);

	// Generate MAC Address
	LOGTRACE("%s memset(m_mac_addr, 0x00, 6);", __PRETTY_FUNCTION__);
	memset(m_mac_addr, 0x00, 6);

	// if (m_bTapEnable) {
	// 	tap->GetMacAddr(m_mac_addr);
	// 	m_mac_addr[5]++;
	// }
	// !!!!!!!!!!!!!!!!! For now, hard code the MAC address. Its annoying when it keeps changing during development!
	// TODO: Remove this hard-coded address
	LOGTRACE("%s m_mac_addr[0]=0x00;", __PRETTY_FUNCTION__);
	m_mac_addr[0]=0x00;
	m_mac_addr[1]=0x80;
	m_mac_addr[2]=0x19;
	m_mac_addr[3]=0x10;
	m_mac_addr[4]=0x98;
	m_mac_addr[5]=0xE3;

#endif	// linux

	AddCommand(SCSIDEV::eCmdRead6, "CmdRead6", &SCSIDaynaPort::CmdRead6);
	AddCommand(SCSIDEV::eCmdWrite6, "CmdWrite6", &SCSIDaynaPort::CmdWrite6);
	AddCommand(SCSIDEV::eCmdRetrieveStats, "CmdRetrieveStats", &SCSIDaynaPort::CmdRetrieveStats);
	AddCommand(SCSIDEV::eCmdSetIfaceMode, "CmdSetIfaceMode", &SCSIDaynaPort::CmdSetIfaceMode);
	AddCommand(SCSIDEV::eCmdSetMcastAddr, "CmdSetMcastAddr", &SCSIDaynaPort::CmdSetMcastAddr);
	AddCommand(SCSIDEV::eCmdEnableInterface, "CmdEnableInterface", &SCSIDaynaPort::CmdEnableInterface);
	AddCommand(SCSIDEV::eCmdGetEventStatusNotification, "CmdGetEventStatusNotification", &SCSIDaynaPort::CmdGetEventStatusNotification);
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SCSIDaynaPort::~SCSIDaynaPort()
{
	// TAP driver release
	if (m_tap) {
		m_tap->Cleanup();
		delete m_tap;
	}

	for (auto const& command : commands) {
		free(command.second);
	}
}

void SCSIDaynaPort::Open(const Filepath& path, BOOL attn)
{
	LOGTRACE("SCSIDaynaPort Open");

	m_tap->OpenDump(path);
}

 void SCSIDaynaPort::AddCommand(SCSIDEV::scsi_command opcode, const char* name, void (SCSIDaynaPort::*execute)(SASIDEV *))
 {
 	commands[opcode] = new command_t(name, execute);
 }

 bool SCSIDaynaPort::Dispatch(SCSIDEV *controller)
 {
 	ctrl = controller->GetWorkAddr();

 	if (commands.count(static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0]))) {
 		command_t *command = commands[static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0])];

 		LOGDEBUG("%s received %s ($%02X)", __PRETTY_FUNCTION__, command->name, (unsigned int)ctrl->cmd[0]);

 		(this->*command->execute)(controller);

 		return true;
 	}

	// The base class handles the less specific commands
 	return Disk::Dispatch(controller);
 }

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SCSIDaynaPort::Inquiry(const DWORD *cdb, BYTE *buffer)
{
	// scsi_cdb_6_byte_t command;
	// memcpy(&command,cdb,sizeof(command));

	ASSERT(cdb);
	ASSERT(buffer);

	//allocation_length = command->length;
	DWORD allocation_length = cdb[4] + (((DWORD)cdb[3]) << 8);
	// if(allocation_length != command.length){
	// 	LOGDEBUG("%s CDB: %02X %02X %02X %02X %02X %02X", __PRETTY_FUNCTION__, (unsigned int)cdb[0], (unsigned int)cdb[1], (unsigned int)cdb[2], (unsigned int)cdb[3], (unsigned int)cdb[4], (unsigned int)cdb[5] );
	// 	LOGWARN(":::::::::: Expected allocation length %04X but found %04X", (unsigned int)allocation_length, (unsigned int)command.length);
	// 	LOGWARN(":::::::::: Doing runtime pointer conversion: %04X", ((scsi_cdb_6_byte_t*)cdb)->length);
	// }

	LOGTRACE("%s Inquiry, allocation length: %d",__PRETTY_FUNCTION__, (int)allocation_length);

	if (allocation_length > 4){
		if (allocation_length > sizeof(m_daynaport_inquiry_response)) {
			allocation_length = sizeof(m_daynaport_inquiry_response);
		}

		// Copy the pre-canned response
		memcpy(buffer, m_daynaport_inquiry_response, allocation_length);

		// Padded vendor, product, revision
		memcpy(&buffer[8], GetPaddedName().c_str(), 28);
	}

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if ((cdb[1] >> 5) & 0x07) {
		buffer[0] |= 0x7f;
	}

	LOGTRACE("response size is %d", (int)allocation_length);

	return allocation_length;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int SCSIDaynaPort::Read(const DWORD *cdb, BYTE *buf, DWORD block)
{
	WORD requested_length = 0;
	int rx_packet_size = 0;
	BOOL send_message_to_host;
	scsi_resp_read_t *response = (scsi_resp_read_t*)buf;
	scsi_cmd_read_6_t *command = (scsi_cmd_read_6_t*)cdb;
	int read_count = 0;

	ASSERT(buf);

	ostringstream s;
	s << __PRETTY_FUNCTION__ << " reading DaynaPort block " << block;
	LOGTRACE("%s", s.str().c_str());

	if(command->operation_code != 0x08){
		LOGERROR("Received unexpected cdb command: %02X. Expected 0x08", (unsigned int)command->operation_code);
	}

	requested_length = (WORD)command->transfer_length;
	LOGTRACE("%s Read maximum length %d, (%04X)", __PRETTY_FUNCTION__, (unsigned int)requested_length, (unsigned int)requested_length);


	// At host startup, it will send a READ(6) command with a length of 1. We should 
	// respond by going into the status mode with a code of 0x02
	if(requested_length == 1){
		return 0;
	}

	// Some of the packets we receive will not be for us. So, we'll keep pulling messages
	// until the buffer is empty, or we've read X times. (X is just a made up number)
	while(read_count < MAX_READ_RETRIES)
	{
		read_count++;

		// The first 2 bytes are reserved for the length of the packet
		// The next 4 bytes are reserved for a flag field
		//rx_packet_size = m_tap->Rx(response->data);
		rx_packet_size = m_tap->Rx(&buf[DAYNAPORT_READ_HEADER_SZ]);

		// If we didn't receive anything, return size of 0
		if(rx_packet_size <= 0){
			LOGTRACE("%s No packet received", __PRETTY_FUNCTION__);
			response->length = 0;
			response->flags = e_no_more_data;
			return DAYNAPORT_READ_HEADER_SZ;
		}

		LOGTRACE("%s Packet Sz %d (%08X) read: %d", __PRETTY_FUNCTION__, (unsigned int) rx_packet_size, (unsigned int) rx_packet_size, read_count);

		// This is a very basic filter to prevent unnecessary packets from
		// being sent to the SCSI initiator. 
		send_message_to_host = FALSE;

	// The following doesn't seem to work with unicast messages. Temporarily removing the filtering
	// functionality.
	///////	// Check if received packet destination MAC address matches the
	///////	// DaynaPort MAC. For IP packets, the mac_address will be the first 6 bytes
	///////	// of the data.
	///////	if (memcmp(response->data, m_mac_addr, 6) == 0) {
	///////		send_message_to_host = TRUE;
	///////	}

	///////	// Check to see if this is a broadcast message
	///////	if (memcmp(response->data, m_bcast_addr, 6) == 0) {
	///////		send_message_to_host = TRUE;
	///////	}

	///////	// Check to see if this is an AppleTalk Message
	///////	if (memcmp(response->data, m_apple_talk_addr, 6) == 0) {
	///////		send_message_to_host = TRUE;
	///////	}
		send_message_to_host = TRUE;

		// TODO: We should check to see if this message is in the multicast 
		// configuration from SCSI command 0x0D

		if(!send_message_to_host){
			LOGDEBUG("%s Received a packet that's not for me: %02X %02X %02X %02X %02X %02X", \
				__PRETTY_FUNCTION__,
				(int)response->data[0],
				(int)response->data[1],
				(int)response->data[2],
				(int)response->data[3],
				(int)response->data[4],
				(int)response->data[5]);

			// If there are pending packets to be processed, we'll tell the host that the read
			// length was 0.
			if(!m_tap->PendingPackets())
			{
				response->length = 0;
				response->flags = e_no_more_data;
				return DAYNAPORT_READ_HEADER_SZ;
			}
		}
		else
		{

			// TODO: Need to do some sort of size checking. The buffer can easily overflow, probably.


			// response->length = rx_packet_size;
			// if(m_tap->PendingPackets()){
			// 	response->flags = e_more_data_available;
			// } else {
			// 	response->flags = e_no_more_data;
			// }
			buf[0] = (BYTE)((rx_packet_size >> 8) & 0xFF);
			buf[1] = (BYTE)(rx_packet_size & 0xFF);

			buf[2] = 0;
			buf[3] = 0;
			buf[4] = 0;
			if(m_tap->PendingPackets()){
				buf[5] = 0x10;
			} else {
				buf[5] = 0;
			}

			// Return the packet size + 2 for the length + 4 for the flag field
			// The CRC was already appended by the ctapdriver
			return rx_packet_size + DAYNAPORT_READ_HEADER_SZ;
		}
		// If we got to this point, there are still messages in the queue, so 
		// we should loop back and get the next one.
	} // end while

	response->length = 0;
	response->flags = e_no_more_data;
	return DAYNAPORT_READ_HEADER_SZ;
}

//---------------------------------------------------------------------------
//
// WRITE check
//
//---------------------------------------------------------------------------
int SCSIDaynaPort::WriteCheck(DWORD block)
{
	ostringstream s;
	s << __PRETTY_FUNCTION__ << " block: " << block;
	LOGTRACE("%s", s.str().c_str());

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	if(!m_bTapEnable){
		SetStatusCode(STATUS_NOTREADY);
		return 0;
	}

	//  Success
	return 1;
}
	
//---------------------------------------------------------------------------
//
//  Write
//
//---------------------------------------------------------------------------
bool SCSIDaynaPort::Write(const DWORD *cdb, const BYTE *buf, DWORD block)
{
	BYTE data_format;
	WORD data_length;
	// const scsi_cmd_daynaport_write_t* command = (const scsi_cmd_daynaport_write_t*)cdb;

	data_format = cdb[5];
	data_length = (WORD)cdb[4] + ((WORD)cdb[3] << 8);

	// if(data_format != command->format){
	// 	LOGDEBUG("%s CDB: %02X %02X %02X %02X %02X %02X", __PRETTY_FUNCTION__, (unsigned int)cdb[0], (unsigned int)cdb[1], (unsigned int)cdb[2], (unsigned int)cdb[3], (unsigned int)cdb[4], (unsigned int)cdb[5] );
	// 	LOGWARN("Expected data_format: %02X, but found %02X", (unsigned int)cdb[5], (unsigned int)command->format);
	// }
	// if(data_length != command->length){
	// 	LOGDEBUG("%s CDB: %02X %02X %02X %02X %02X %02X", __PRETTY_FUNCTION__, (unsigned int)cdb[0], (unsigned int)cdb[1], (unsigned int)cdb[2], (unsigned int)cdb[3], (unsigned int)cdb[4], (unsigned int)cdb[5] );
	// 	LOGWARN("Expected data_length: %04X, but found %04X", data_length, (unsigned int)command->length);
	// }

	if(data_format == 0x00){
		m_tap->Tx(buf, data_length);
		LOGTRACE("%s Transmitted %u bytes (00 format)", __PRETTY_FUNCTION__, data_length);
		return true;
	}
	else if (data_format == 0x80){
		// The data length is actuall specified in the first 2 bytes of the payload
		data_length=(WORD)buf[1] + ((WORD)buf[0] << 8);
		m_tap->Tx(&buf[4], data_length);
		LOGTRACE("%s Transmitted %u bytes (80 format)", __PRETTY_FUNCTION__, data_length);
		return true;
	}
	else
	{
		// LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)command->format);
		LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)data_format);
		return true;
	}
}
	

//---------------------------------------------------------------------------
//
//	RetrieveStats
//
//---------------------------------------------------------------------------
int SCSIDaynaPort::RetrieveStats(const DWORD *cdb, BYTE *buffer)
{
	DWORD response_size;
	DWORD allocation_length;

	// DWORD frame_alignment_errors;
	// DWORD crc_errors;
	// DWORD frames_lost;

	ASSERT(cdb);
	ASSERT(buffer);

	allocation_length = cdb[4] + (((DWORD)cdb[3]) << 8);
	LOGTRACE("%s Retrieve Stats buffer size was %d", __PRETTY_FUNCTION__, (int)allocation_length);

	// // ASSERT(cdb[0] == 0x09);

	// if(cdb[0] != 0x09)
	// {
	// 	LOGWARN("%s cdb[0] was not 0x09, as I expected. It was %02X.", __PRETTY_FUNCTION__, (unsigned int)cdb[0]);
	// }
	// if(cdb[4] != 0x12)
	// {
	// 	LOGWARN("%s cdb[4] was not 0x12, as I expected. It was %02X.", __PRETTY_FUNCTION__, (unsigned int)cdb[4]);
	// }

	// memset(buffer,0,18);

	// memcpy(&buffer[0],m_mac_addr,sizeof(m_mac_addr));
	// // frame alignment errors
	// frame_alignment_errors = htonl(0);
	// memcpy(&(buffer[6]),&frame_alignment_errors,sizeof(frame_alignment_errors));
	// // CRC errors
	// crc_errors = htonl(0);
	// memcpy(&(buffer[10]),&crc_errors,sizeof(crc_errors));
	// // frames lost
	// frames_lost = htonl(0);
	// memcpy(&(buffer[14]),&frames_lost,sizeof(frames_lost));

	for(int i=0; i< 6; i++)
	{
		LOGTRACE("%s CDB byte %d: %02X",__PRETTY_FUNCTION__, i, (unsigned int)cdb[i]);
	}

	response_size = 18;

	response_size = sizeof(m_scsi_link_stats);
	memcpy(buffer, &m_scsi_link_stats, sizeof(m_scsi_link_stats));

	LOGTRACE("%s response size is %d", __PRETTY_FUNCTION__, (int)response_size);

	if(response_size > allocation_length)
	{
		response_size = allocation_length;
		LOGINFO("%s Truncating the inquiry response", __PRETTY_FUNCTION__)
	}

	//  Success
	return response_size;
	// scsi_cdb_6_byte_t *command = (scsi_cdb_6_byte_t*)cdb;
	// scsi_resp_link_stats_t *response = (scsi_resp_link_stats_t*) buffer;

	// LOGTRACE("%s Retrieve Stats buffer size was %d", __PRETTY_FUNCTION__, command->length);

	// ASSERT(sizeof(scsi_resp_link_stats_t) == 18);

	// memcpy(response->mac_address, m_mac_addr, sizeof(m_mac_addr));
	// response->crc_errors = 0;
	// response->frames_lost = 0;
	// response->frame_alignment_errors = 0;

	// //  Success
	// disk.code = DISK_NOERROR;
	// return sizeof(scsi_resp_link_stats_t);
}

//---------------------------------------------------------------------------
//
//	Enable or Disable the interface
//
//---------------------------------------------------------------------------
bool SCSIDaynaPort::EnableInterface(const DWORD *cdb)
{
	bool result;
	if (cdb[5] & 0x80) {
		result = m_tap->Enable();
		if (result) {
			LOGINFO("The DaynaPort interface has been ENABLED.");
		}
		else{
			LOGWARN("Unable to enable the DaynaPort Interface");
		}
		m_tap->Flush();
	}
	else {
		result = m_tap->Disable();
		if (result) {
			LOGINFO("The DaynaPort interface has been DISABLED.");
		}
		else{
			LOGWARN("Unable to disable the DaynaPort Interface");
		}
	}

	return result;
}

bool SCSIDaynaPort::TestUnitReady(const DWORD* /*cdb*/)
{
	// TEST UNIT READY Success
	return true;
}

void SCSIDaynaPort::CmdRead6(SASIDEV *controller)
{
	// Get record number and block number
	DWORD record = ctrl->cmd[1] & 0x1f;
	record <<= 8;
	record |= ctrl->cmd[2];
	record <<= 8;
	record |= ctrl->cmd[3];
	ctrl->blocks=1;

	LOGTRACE("%s READ(6) command record=%d blocks=%d", __PRETTY_FUNCTION__, (unsigned int)record, (int)ctrl->blocks);

	ctrl->length = Read(ctrl->cmd, ctrl->buffer, record);
	LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, (int)ctrl->length);

	// Set next block
	ctrl->next = record + 1;

	// Read phase
	controller->DataIn();
}

void SCSIDaynaPort::CmdWrite6(SASIDEV *controller)
{
	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl->bufsize < DAYNAPORT_BUFFER_SIZE) {
		free(ctrl->buffer);
		ctrl->bufsize = DAYNAPORT_BUFFER_SIZE;
		ctrl->buffer = (BYTE *)malloc(ctrl->bufsize);
	}

	DWORD data_format = ctrl->cmd[5];

	if(data_format == 0x00){
		ctrl->length = (WORD)ctrl->cmd[4] + ((WORD)ctrl->cmd[3] << 8);
	}
	else if (data_format == 0x80){
		ctrl->length = (WORD)ctrl->cmd[4] + ((WORD)ctrl->cmd[3] << 8) + 8;
	}
	else
	{
		LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)data_format);
	}
	LOGTRACE("%s length: %04X (%d) format: %02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->length, (int)ctrl->length, (unsigned int)data_format);

	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	// Data out phase
	controller->DataOut();
}

void SCSIDaynaPort::CmdRetrieveStats(SASIDEV *controller)
{
	ctrl->length = RetrieveStats(ctrl->cmd, ctrl->buffer);

	if (ctrl->length <= 0) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	// Data in phase
	controller->DataIn();
}

void SCSIDaynaPort::CmdSetIfaceMode(SASIDEV *controller)
{
	// Check whether this command is telling us to "Set Interface Mode" or "Set MAC Address"

	ctrl->length = RetrieveStats(ctrl->cmd, ctrl->buffer);
	switch(ctrl->cmd[5]){
		case SCSIDaynaPort::CMD_SCSILINK_SETMODE:
			SetMode(ctrl->cmd, ctrl->buffer);
			controller->Status();
			break;
		break;
		case SCSIDaynaPort::CMD_SCSILINK_SETMAC:
			ctrl->length = 6;
			// Write phase
			controller->DataOut();
			break;
		default:
			LOGWARN("%s Unknown SetInterface command received: %02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[5]);
	}
}

void SCSIDaynaPort::CmdSetMcastAddr(SASIDEV *controller)
{
	ctrl->length = (DWORD)ctrl->cmd[4];

	// ASSERT(ctrl.length >= 0);
	if (ctrl->length == 0) {
		LOGWARN("%s Not supported SetMcastAddr Command %02X", __PRETTY_FUNCTION__, (WORD)ctrl->cmd[2]);

		// Failure (Error)
		controller->Error();
		return;
	}

	controller->DataOut();
}

void SCSIDaynaPort::CmdEnableInterface(SASIDEV *controller)
{
	bool status = EnableInterface(ctrl->cmd);
	if (!status) {
		// Failure (Error)
		controller->Error();
		return;
	}

	// status phase
	controller->Status();
}

void SCSIDaynaPort::CmdGetEventStatusNotification(SASIDEV *controller)
{
	// This naive (but legal) implementation avoids constant warnings in the logs
	controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
}

//---------------------------------------------------------------------------
//
//	Set Mode - enable broadcast messages
//
//---------------------------------------------------------------------------
void SCSIDaynaPort::SetMode(const DWORD *cdb, BYTE *buffer)
{
	LOGTRACE("%s Setting mode", __PRETTY_FUNCTION__);

	for(size_t i=0; i<sizeof(6); i++)
	{
		LOGTRACE("%s %d: %02X",__PRETTY_FUNCTION__, (unsigned int)i,(WORD)cdb[i]);
	}	
}


