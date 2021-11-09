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
//  Note: This requires a DaynaPort SCSI Link driver.
//---------------------------------------------------------------------------

#include "scsi_daynaport.h"
#include <sstream>

const BYTE SCSIDaynaPort::m_bcast_addr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const BYTE SCSIDaynaPort::m_apple_talk_addr[6] = { 0x09, 0x00, 0x07, 0xff, 0xff, 0xff };

SCSIDaynaPort::SCSIDaynaPort() : Disk("SCDP")
{
	m_tap = NULL;
	m_bTapEnable = false;

	AddCommand(SCSIDEV::eCmdTestUnitReady, "TestUnitReady", &SCSIDaynaPort::TestUnitReady);
	AddCommand(SCSIDEV::eCmdRead6, "Read6", &SCSIDaynaPort::Read6);
	AddCommand(SCSIDEV::eCmdWrite6, "Write6", &SCSIDaynaPort::Write6);
	AddCommand(SCSIDEV::eCmdRetrieveStats, "RetrieveStats", &SCSIDaynaPort::RetrieveStatistics);
	AddCommand(SCSIDEV::eCmdSetIfaceMode, "SetIfaceMode", &SCSIDaynaPort::SetInterfaceMode);
	AddCommand(SCSIDEV::eCmdSetMcastAddr, "SetMcastAddr", &SCSIDaynaPort::SetMcastAddr);
	AddCommand(SCSIDEV::eCmdEnableInterface, "EnableInterface", &SCSIDaynaPort::EnableInterface);
}

SCSIDaynaPort::~SCSIDaynaPort()
{
	// TAP driver release
	if (m_tap) {
		m_tap->Cleanup();
		delete m_tap;
	}

	for (auto const& command : commands) {
		delete command.second;
	}
}

void SCSIDaynaPort::AddCommand(SCSIDEV::scsi_command opcode, const char* name, void (SCSIDaynaPort::*execute)(SASIDEV *))
{
	commands[opcode] = new command_t(name, execute);
}

bool SCSIDaynaPort::Dispatch(SCSIDEV *controller)
{
	ctrl = controller->GetCtrl();

	if (commands.count(static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0]))) {
		command_t *command = commands[static_cast<SCSIDEV::scsi_command>(ctrl->cmd[0])];

		LOGDEBUG("%s Executing %s ($%02X)", __PRETTY_FUNCTION__, command->name, (unsigned int)ctrl->cmd[0]);

		(this->*command->execute)(controller);

		return true;
	}

	LOGTRACE("%s Calling base class for dispatching $%02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[0]);

	// The base class handles the less specific commands
	return Disk::Dispatch(controller);
}

bool SCSIDaynaPort::Init(const map<string, string>& params)
{
	SetParams(params.empty() ? GetDefaultParams() : params);

#ifdef __linux__
	// TAP Driver Generation
	m_tap = new CTapDriver(GetParam("interfaces"));
	m_bTapEnable = m_tap->Init();
	if(!m_bTapEnable){
		LOGERROR("Unable to open the TAP interface");

// Not terminating on regular Linux PCs is helpful for testing
#if !defined(__x86_64__) && !defined(__X86__)
		return false;
#endif
	} else {
		LOGDEBUG("Tap interface created");
	}

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

	return true;
}

void SCSIDaynaPort::Open(const Filepath& path)
{
	m_tap->OpenDump(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int SCSIDaynaPort::Inquiry(const DWORD *cdb, BYTE *buf)
{
	int allocation_length = cdb[4] + (((DWORD)cdb[3]) << 8);

	LOGTRACE("%s Inquiry, allocation length: %d",__PRETTY_FUNCTION__, allocation_length);

	if (allocation_length > 4) {
		if (allocation_length > 44) {
			allocation_length = 44;
		}

		// Basic data
		// buf[0] ... Processor Device
		// buf[1] ... Not removable
		// buf[2] ... SCSI-2 compliant command system
		// buf[3] ... SCSI-2 compliant Inquiry response
		// buf[4] ... Inquiry additional data
		// http://www.bitsavers.org/pdf/apple/scsi/dayna/daynaPORT/pocket_scsiLINK/pocketscsilink_inq.png
		memset(buf, 0, allocation_length);
		buf[0] = 0x03;
		buf[2] = 0x01;
		buf[4] = 0x1F;

		// Padded vendor, product, revision
		memcpy(&buf[8], GetPaddedName().c_str(), 28);
	}

	LOGTRACE("response size is %d", allocation_length);

	return allocation_length;
}

//---------------------------------------------------------------------------
//
//	READ
//
//   Command:  08 00 00 LL LL XX (LLLL is data length, XX = c0 or 80)
//   Function: Read a packet at a time from the device (standard SCSI Read)
//   Type:     Input; the following data is returned:
//             LL LL NN NN NN NN XX XX XX ... CC CC CC CC
//   where:
//             LLLL      is normally the length of the packet (a 2-byte
//                       big-endian hex value), including 4 trailing bytes
//                       of CRC, but excluding itself and the flag field.
//                       See below for special values
//             NNNNNNNN  is a 4-byte flag field with the following meanings:
//                       FFFFFFFF  a packet has been dropped (?); in this case
//                                 the length field appears to be always 4000
//                       00000010  there are more packets currently available
//                                 in SCSI/Link memory
//                       00000000  this is the last packet
//             XX XX ... is the actual packet
//             CCCCCCCC  is the CRC
//
//   Notes:
//    - When all packets have been retrieved successfully, a length field
//      of 0000 is returned; however, if a packet has been dropped, the
//      SCSI/Link will instead return a non-zero length field with a flag
//      of FFFFFFFF when there are no more packets available.  This behaviour
//      seems to continue until a disable/enable sequence has been issued.
//    - The SCSI/Link apparently has about 6KB buffer space for packets.
//
//---------------------------------------------------------------------------
int SCSIDaynaPort::Read(const DWORD *cdb, BYTE *buf, uint64_t block)
{
	int rx_packet_size = 0;
	scsi_resp_read_t *response = (scsi_resp_read_t*)buf;

	ostringstream s;
	s << __PRETTY_FUNCTION__ << " reading DaynaPort block " << block;
	LOGTRACE("%s", s.str().c_str());

	if (cdb[0] != 0x08) {
		LOGERROR("Received unexpected cdb command: %02X. Expected 0x08", cdb[0]);
	}

	int requested_length = cdb[4];
	LOGTRACE("%s Read maximum length %d, (%04X)", __PRETTY_FUNCTION__, requested_length, requested_length);


	// At host startup, it will send a READ(6) command with a length of 1. We should 
	// respond by going into the status mode with a code of 0x02
	if (requested_length == 1) {
		return 0;
	}

	// Some of the packets we receive will not be for us. So, we'll keep pulling messages
	// until the buffer is empty, or we've read X times. (X is just a made up number)
	bool send_message_to_host;
	int read_count = 0;
	while (read_count < MAX_READ_RETRIES) {
		read_count++;

		// The first 2 bytes are reserved for the length of the packet
		// The next 4 bytes are reserved for a flag field
		//rx_packet_size = m_tap->Rx(response->data);
		rx_packet_size = m_tap->Rx(&buf[DAYNAPORT_READ_HEADER_SZ]);

		// If we didn't receive anything, return size of 0
		if (rx_packet_size <= 0) {
			LOGTRACE("%s No packet received", __PRETTY_FUNCTION__);
			response->length = 0;
			response->flags = e_no_more_data;
			return DAYNAPORT_READ_HEADER_SZ;
		}

		LOGTRACE("%s Packet Sz %d (%08X) read: %d", __PRETTY_FUNCTION__, (unsigned int) rx_packet_size, (unsigned int) rx_packet_size, read_count);

		// This is a very basic filter to prevent unnecessary packets from
		// being sent to the SCSI initiator. 
		send_message_to_host = false;

	// The following doesn't seem to work with unicast messages. Temporarily removing the filtering
	// functionality.
	///////	// Check if received packet destination MAC address matches the
	///////	// DaynaPort MAC. For IP packets, the mac_address will be the first 6 bytes
	///////	// of the data.
	///////	if (memcmp(response->data, m_mac_addr, 6) == 0) {
	///////		send_message_to_host = true;
	///////	}

	///////	// Check to see if this is a broadcast message
	///////	if (memcmp(response->data, m_bcast_addr, 6) == 0) {
	///////		send_message_to_host = true;
	///////	}

	///////	// Check to see if this is an AppleTalk Message
	///////	if (memcmp(response->data, m_apple_talk_addr, 6) == 0) {
	///////		send_message_to_host = true;
	///////	}
		send_message_to_host = true;

		// TODO: We should check to see if this message is in the multicast 
		// configuration from SCSI command 0x0D

		if (!send_message_to_host) {
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
			if (!m_tap->PendingPackets())
			{
				response->length = 0;
				response->flags = e_no_more_data;
				return DAYNAPORT_READ_HEADER_SZ;
			}
		}
		else {
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
	// Status check
	if (!CheckReady()) {
		return 0;
	}

	if (!m_bTapEnable){
		SetStatusCode(STATUS_NOTREADY);
		return 0;
	}

	// Success
	return 1;
}
	
//---------------------------------------------------------------------------
//
//  Write
//
//   Command:  0a 00 00 LL LL XX (LLLL is data length, XX = 80 or 00)
//   Function: Write a packet at a time to the device (standard SCSI Write)
//   Type:     Output; the format of the data to be sent depends on the value
//             of XX, as follows:
//              - if XX = 00, LLLL is the packet length, and the data to be sent
//                must be an image of the data packet
//              - if XX = 80, LLLL is the packet length + 8, and the data to be
//                sent is:
//                  PP PP 00 00 XX XX XX ... 00 00 00 00
//                where:
//                  PPPP      is the actual (2-byte big-endian) packet length
//               XX XX ... is the actual packet
//
//---------------------------------------------------------------------------
bool SCSIDaynaPort::Write(const DWORD *cdb, const BYTE *buf, DWORD block)
{
	BYTE data_format = cdb[5];
	WORD data_length = (WORD)cdb[4] + ((WORD)cdb[3] << 8);

	if (data_format == 0x00){
		m_tap->Tx(buf, data_length);
		LOGTRACE("%s Transmitted %u bytes (00 format)", __PRETTY_FUNCTION__, data_length);
		return true;
	}
	else if (data_format == 0x80){
		// The data length is specified in the first 2 bytes of the payload
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
//   Command:  09 00 00 00 12 00
//   Function: Retrieve MAC address and device statistics
//   Type:     Input; returns 18 (decimal) bytes of data as follows:
//              - bytes 0-5:  the current hardware ethernet (MAC) address
//              - bytes 6-17: three long word (4-byte) counters (little-endian).
//   Notes:    The contents of the three longs are typically zero, and their
//             usage is unclear; they are suspected to be:
//              - long #1: frame alignment errors
//              - long #2: CRC errors
//              - long #3: frames lost
//
//---------------------------------------------------------------------------
int SCSIDaynaPort::RetrieveStats(const DWORD *cdb, BYTE *buffer)
{
	int allocation_length = cdb[4] + (((DWORD)cdb[3]) << 8);
	LOGTRACE("%s Retrieve Stats buffer size is %d", __PRETTY_FUNCTION__, (int)allocation_length);

	// if(cdb[4] != 0x12)
	// {
	// 	LOGWARN("%s cdb[4] was not 0x12, as I expected. It was %02X.", __PRETTY_FUNCTION__, (unsigned int)cdb[4]);
	// }
	// memset(buffer,0,18);
	// memcpy(&buffer[0],m_mac_addr,sizeof(m_mac_addr));
	// uint32_t frame_alignment_errors;
	// uint32_t crc_errors;
	// uint32_t frames_lost;
	// // frame alignment errors
	// frame_alignment_errors = htonl(0);
	// memcpy(&(buffer[6]),&frame_alignment_errors,sizeof(frame_alignment_errors));
	// // CRC errors
	// crc_errors = htonl(0);
	// memcpy(&(buffer[10]),&crc_errors,sizeof(crc_errors));
	// // frames lost
	// frames_lost = htonl(0);
	// memcpy(&(buffer[14]),&frames_lost,sizeof(frames_lost));

	for (int i = 0; i < 6; i++) {
		LOGTRACE("%s CDB byte %d: %02X",__PRETTY_FUNCTION__, i, (unsigned int)cdb[i]);
	}

	int response_size = sizeof(m_scsi_link_stats);
	memcpy(buffer, &m_scsi_link_stats, sizeof(m_scsi_link_stats));

	LOGTRACE("%s response size is %d", __PRETTY_FUNCTION__, (int)response_size);

	if (response_size > allocation_length) {
		response_size = allocation_length;
		LOGINFO("%s Truncating the inquiry response", __PRETTY_FUNCTION__)
	}

	// Success
	return response_size;
}

//---------------------------------------------------------------------------
//
//	Enable or Disable the interface
//
//  Command:  0e 00 00 00 00 XX (XX = 80 or 00)
//  Function: Enable (80) / disable (00) Ethernet interface
//  Type:     No data transferred
//  Notes:    After issuing an Enable, the initiator should avoid sending
//            any subsequent commands to the device for approximately 0.5
//            seconds
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

void SCSIDaynaPort::TestUnitReady(SASIDEV *controller)
{
	// TEST UNIT READY Success

	controller->Status();
}

void SCSIDaynaPort::Read6(SASIDEV *controller)
{
	// Get record number and block number
	uint32_t record = ctrl->cmd[1] & 0x1f;
	record <<= 8;
	record |= ctrl->cmd[2];
	record <<= 8;
	record |= ctrl->cmd[3];
	ctrl->blocks=1;

	// If any commands have a bogus control value, they were probably not
	// generated by the DaynaPort driver so ignore them
	if (ctrl->cmd[5] != 0xc0 && ctrl->cmd[5] != 0x80) {
		LOGTRACE("%s Control value %d, (%04X), returning invalid CDB", __PRETTY_FUNCTION__, ctrl->cmd[5], ctrl->cmd[5]);
		controller->Error(ERROR_CODES::sense_key::ILLEGAL_REQUEST, ERROR_CODES::asc::INVALID_FIELD_IN_CDB);
		return;
	}

	LOGTRACE("%s READ(6) command record=%d blocks=%d", __PRETTY_FUNCTION__, (unsigned int)record, (int)ctrl->blocks);

	ctrl->length = Read(ctrl->cmd, ctrl->buffer, record);
	LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, (int)ctrl->length);

	// Set next block
	ctrl->next = record + 1;

	controller->DataIn();
}

void SCSIDaynaPort::Write6(SASIDEV *controller)
{
	// Reallocate buffer (because it is not transfer for each block)
	if (ctrl->bufsize < DAYNAPORT_BUFFER_SIZE) {
		free(ctrl->buffer);
		ctrl->bufsize = DAYNAPORT_BUFFER_SIZE;
		ctrl->buffer = (BYTE *)malloc(ctrl->bufsize);
	}

	DWORD data_format = ctrl->cmd[5];

	if(data_format == 0x00) {
		ctrl->length = (WORD)ctrl->cmd[4] + ((WORD)ctrl->cmd[3] << 8);
	}
	else if (data_format == 0x80) {
		ctrl->length = (WORD)ctrl->cmd[4] + ((WORD)ctrl->cmd[3] << 8) + 8;
	}
	else {
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

	controller->DataOut();
}

void SCSIDaynaPort::RetrieveStatistics(SASIDEV *controller)
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

	controller->DataIn();
}

//---------------------------------------------------------------------------
//
//	Set interface mode/Set MAC address
//
//   Set Interface Mode (0c)
//   -----------------------
//   Command:  0c 00 00 00 FF 80 (FF = 08 or 04)
//   Function: Allow interface to receive broadcast messages (FF = 04); the
//             function of (FF = 08) is currently unknown.
//   Type:     No data transferred
//   Notes:    This command is accepted by firmware 1.4a & 2.0f, but has no
//             effect on 2.0f, which is always capable of receiving broadcast
//             messages.  In 1.4a, once broadcast mode is set, it remains set
//             until the interface is disabled.
//
//   Set MAC Address (0c)
//   --------------------
//   Command:  0c 00 00 00 FF 40 (FF = 08 or 04)
//   Function: Set MAC address
//   Type:     Output; overrides built-in MAC address with user-specified
//             6-byte value
//   Notes:    This command is intended primarily for debugging/test purposes.
//             Disabling the interface resets the MAC address to the built-in
//             value.
//
//---------------------------------------------------------------------------
void SCSIDaynaPort::SetInterfaceMode(SASIDEV *controller)
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
			controller->DataOut();
			break;

		default:
			LOGWARN("%s Unknown SetInterface command received: %02X", __PRETTY_FUNCTION__, (unsigned int)ctrl->cmd[5]);
			break;
	}
}

void SCSIDaynaPort::SetMcastAddr(SASIDEV *controller)
{
	ctrl->length = (DWORD)ctrl->cmd[4];
	if (ctrl->length == 0) {
		LOGWARN("%s Not supported SetMcastAddr Command %02X", __PRETTY_FUNCTION__, (WORD)ctrl->cmd[2]);

		// Failure (Error)
		controller->Error();
		return;
	}

	controller->DataOut();
}

void SCSIDaynaPort::EnableInterface(SASIDEV *controller)
{
	bool status = EnableInterface(ctrl->cmd);
	if (!status) {
		// Failure (Error)
		controller->Error();
		return;
	}

	controller->Status();
}

//---------------------------------------------------------------------------
//
//	Set Mode - enable broadcast messages
//
//---------------------------------------------------------------------------
void SCSIDaynaPort::SetMode(const DWORD *cdb, BYTE *buffer)
{
	LOGTRACE("%s Setting mode", __PRETTY_FUNCTION__);

	for (int i = 0; i < 6; i++) {
		LOGTRACE("%s %d: %02X",__PRETTY_FUNCTION__, (unsigned int)i, (int)cdb[i]);
	}	
}


