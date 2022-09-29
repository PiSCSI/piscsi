//---------------------------------------------------------------------------
//
//	SCSI Target Emulator RaSCSI Reloaded
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

#include "rascsi_exceptions.h"
#include "scsi_command_util.h"
#include "dispatcher.h"
#include "scsi_daynaport.h"

using namespace scsi_defs;
using namespace scsi_command_util;

// TODO Disk must not be the superclass
SCSIDaynaPort::SCSIDaynaPort(int lun) : Disk("SCDP", lun)
{
	dispatcher.Add(scsi_command::eCmdTestUnitReady, "TestUnitReady", &SCSIDaynaPort::TestUnitReady);
	dispatcher.Add(scsi_command::eCmdRead6, "Read6", &SCSIDaynaPort::Read6);
	dispatcher.Add(scsi_command::eCmdWrite6, "Write6", &SCSIDaynaPort::Write6);
	dispatcher.Add(scsi_command::eCmdRetrieveStats, "RetrieveStats", &SCSIDaynaPort::RetrieveStatistics);
	dispatcher.Add(scsi_command::eCmdSetIfaceMode, "SetIfaceMode", &SCSIDaynaPort::SetInterfaceMode);
	dispatcher.Add(scsi_command::eCmdSetMcastAddr, "SetMcastAddr", &SCSIDaynaPort::SetMcastAddr);
	dispatcher.Add(scsi_command::eCmdEnableInterface, "EnableInterface", &SCSIDaynaPort::EnableInterface);
}

bool SCSIDaynaPort::Dispatch(scsi_command cmd)
{
	// TODO As long as DaynaPort suffers from being a subclass of Disk at least reject MODE SENSE and MODE SELECT
	if (cmd == scsi_command::eCmdModeSense6 || cmd == scsi_command::eCmdModeSelect6 ||
			cmd == scsi_command::eCmdModeSense10 || cmd == scsi_command::eCmdModeSelect10) {
		return false;
	}

	// The superclass class handles the less specific commands
	return dispatcher.Dispatch(this, cmd) ? true : super::Dispatch(cmd);
}

bool SCSIDaynaPort::Init(const unordered_map<string, string>& params)
{
	SetParams(params);

	m_bTapEnable = m_tap.Init(GetParams());
	if(!m_bTapEnable){
		LOGERROR("Unable to open the TAP interface")

// Not terminating on regular Linux PCs is helpful for testing
#if !defined(__x86_64__) && !defined(__X86__)
	return false;
#endif
	} else {
		LOGDEBUG("Tap interface created")
	}

	Reset();
	SetReady(true);
	SetReset(false);

	return true;
}

void SCSIDaynaPort::Open(const Filepath& path)
{
	m_tap.OpenDump(path);
}

vector<byte> SCSIDaynaPort::InquiryInternal() const
{
	vector<byte> buf = HandleInquiry(device_type::PROCESSOR, scsi_level::SCSI_2, false);

	// The Daynaport driver for the Mac expects 37 bytes: Increase additional length and
	// add a vendor-specific byte in order to satisfy this driver.
	buf[4] = (byte)((int)buf[4] + 1);
	buf.push_back((byte)0);

	return buf;
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
int SCSIDaynaPort::Read(const vector<int>& cdb, vector<BYTE>& buf, uint64_t)
{
	int rx_packet_size = 0;
	auto response = (scsi_resp_read_t*)buf.data();

	int requested_length = cdb[4];
	LOGTRACE("%s Read maximum length %d, (%04X)", __PRETTY_FUNCTION__, requested_length, requested_length)


	// At host startup, it will send a READ(6) command with a length of 1. We should
	// respond by going into the status mode with a code of 0x02
	if (requested_length == 1) {
		return 0;
	}

	// Some of the packets we receive will not be for us. So, we'll keep pulling messages
	// until the buffer is empty, or we've read X times. (X is just a made up number)
	// TODO send_message_to_host is effctively always true
	bool send_message_to_host;
	int read_count = 0;
	while (read_count < MAX_READ_RETRIES) {
		read_count++;

		// The first 2 bytes are reserved for the length of the packet
		// The next 4 bytes are reserved for a flag field
		//rx_packet_size = m_tap.Rx(response->data);
		rx_packet_size = m_tap.Receive(&buf[DAYNAPORT_READ_HEADER_SZ]);

		// If we didn't receive anything, return size of 0
		if (rx_packet_size <= 0) {
			LOGTRACE("%s No packet received", __PRETTY_FUNCTION__)
			response->length = 0;
			response->flags = read_data_flags_t::e_no_more_data;
			return DAYNAPORT_READ_HEADER_SZ;
		}

		LOGTRACE("%s Packet Sz %d (%08X) read: %d", __PRETTY_FUNCTION__, (unsigned int) rx_packet_size, (unsigned int) rx_packet_size, read_count)

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
				(int)response->data[5])

			// If there are pending packets to be processed, we'll tell the host that the read
			// length was 0.
			if (!m_tap.PendingPackets())
			{
				response->length = 0;
				response->flags = read_data_flags_t::e_no_more_data;
				return DAYNAPORT_READ_HEADER_SZ;
			}
		}
		else {
			// TODO: Need to do some sort of size checking. The buffer can easily overflow, probably.

			// response->length = rx_packet_size;
			// if(m_tap.PendingPackets()){
			// 	response->flags = e_more_data_available;
			// } else {
			// 	response->flags = e_no_more_data;
			// }
			int size = rx_packet_size;
			if (size < 64) {
				// A frame must have at least 64 bytes (see https://github.com/akuker/RASCSI/issues/619)
				// Note that this work-around breaks the checksum. As currently there are no known drivers
				// that care for the checksum, and the Daynaport driver for the Atari expects frames of
				// 64 bytes it was decided to accept the broken checksum. If a driver should pop up that
				// breaks because of this, the work-around has to be re-evaluated.
				size = 64;
			}
			SetInt16(buf, 0, size);
			SetInt32(buf, 2, m_tap.PendingPackets() ? 0x10 : 0x00);

			// Return the packet size + 2 for the length + 4 for the flag field
			// The CRC was already appended by the ctapdriver
			return size + DAYNAPORT_READ_HEADER_SZ;
		}
		// If we got to this point, there are still messages in the queue, so
		// we should loop back and get the next one.
	} // end while

	response->length = 0;
	response->flags = read_data_flags_t::e_no_more_data;
	return DAYNAPORT_READ_HEADER_SZ;
}

int SCSIDaynaPort::WriteCheck(uint64_t)
{
	CheckReady();

	if (!m_bTapEnable){
		throw scsi_error_exception(sense_key::UNIT_ATTENTION, asc::MEDIUM_NOT_PRESENT);
	}

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
bool SCSIDaynaPort::WriteBytes(const vector<int>& cdb, const vector<BYTE>& buf, uint64_t)
{
	int data_format = cdb[5];
	int data_length = GetInt16(cdb, 3);

	if (data_format == 0x00){
		m_tap.Send(buf.data(), data_length);
		LOGTRACE("%s Transmitted %u bytes (00 format)", __PRETTY_FUNCTION__, data_length)
	}
	else if (data_format == 0x80){
		// The data length is specified in the first 2 bytes of the payload
		data_length=buf[1] + (buf[0] << 8);
		m_tap.Send(&(buf.data()[4]), data_length);
		LOGTRACE("%s Transmitted %u bytes (80 format)", __PRETTY_FUNCTION__, data_length)
	}
	else
	{
		// LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, (unsigned int)command->format)
		LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, data_format)
	}

	return true;
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
int SCSIDaynaPort::RetrieveStats(const vector<int>& cdb, vector<BYTE>& buf) const
{
	memcpy(buf.data(), &m_scsi_link_stats, sizeof(m_scsi_link_stats));

	return (int)min(sizeof(m_scsi_link_stats), (size_t)GetInt16(cdb, 4));
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
bool SCSIDaynaPort::EnableInterface(const vector<int>& cdb)
{
	bool result;
	if (cdb[5] & 0x80) {
		result = m_tap.Enable();
		if (result) {
			LOGINFO("The DaynaPort interface has been ENABLED.")
		}
		else{
			LOGWARN("Unable to enable the DaynaPort Interface")
		}
		m_tap.Flush();
	}
	else {
		result = m_tap.Disable();
		if (result) {
			LOGINFO("The DaynaPort interface has been DISABLED.")
		}
		else{
			LOGWARN("Unable to disable the DaynaPort Interface")
		}
	}

	return result;
}

void SCSIDaynaPort::TestUnitReady()
{
	// Always successful
	EnterStatusPhase();
}

void SCSIDaynaPort::Read6()
{
	// Get record number and block number
    uint32_t record = GetInt24(ctrl->cmd, 1) & 0x1fffff;
	ctrl->blocks=1;

	// If any commands have a bogus control value, they were probably not
	// generated by the DaynaPort driver so ignore them
	if (ctrl->cmd[5] != 0xc0 && ctrl->cmd[5] != 0x80) {
		LOGTRACE("%s Control value %d, (%04X), returning invalid CDB", __PRETTY_FUNCTION__, ctrl->cmd[5], ctrl->cmd[5])
		throw scsi_error_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	LOGTRACE("%s READ(6) command record=%d blocks=%d", __PRETTY_FUNCTION__, record, ctrl->blocks)

	ctrl->length = Read(ctrl->cmd, controller->GetBuffer(), record);
	LOGTRACE("%s ctrl.length is %d", __PRETTY_FUNCTION__, ctrl->length)

	// Set next block
	ctrl->next = record + 1;

	EnterDataInPhase();
}

void SCSIDaynaPort::Write6()
{
	// Ensure a sufficient buffer size (because it is not transfer for each block)
	controller->AllocateBuffer(DAYNAPORT_BUFFER_SIZE);

	int data_format = ctrl->cmd[5];

	if (data_format == 0x00) {
		ctrl->length = GetInt16(ctrl->cmd, 3);
	}
	else if (data_format == 0x80) {
		ctrl->length = GetInt16(ctrl->cmd, 3 + 8);
	}
	else {
		LOGWARN("%s Unknown data format %02X", __PRETTY_FUNCTION__, data_format)
	}
	LOGTRACE("%s length: %04X (%d) format: %02X", __PRETTY_FUNCTION__, ctrl->length, ctrl->length, data_format)

	if (ctrl->length <= 0) {
		throw scsi_error_exception();
	}

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	EnterDataOutPhase();
}

void SCSIDaynaPort::RetrieveStatistics()
{
	ctrl->length = RetrieveStats(ctrl->cmd, controller->GetBuffer());

	// Set next block
	ctrl->blocks = 1;
	ctrl->next = 1;

	EnterDataInPhase();
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
void SCSIDaynaPort::SetInterfaceMode()
{
	// Check whether this command is telling us to "Set Interface Mode" or "Set MAC Address"

	ctrl->length = RetrieveStats(ctrl->cmd, controller->GetBuffer());
	switch(ctrl->cmd[5]){
		case CMD_SCSILINK_SETMODE:
			// TODO Not implemented, do nothing
			EnterStatusPhase();
			break;

		case CMD_SCSILINK_SETMAC:
			ctrl->length = 6;
			EnterDataOutPhase();
			break;

		case CMD_SCSILINK_STATS:
		case CMD_SCSILINK_ENABLE:
		case CMD_SCSILINK_SET:
			LOGWARN("%s Unsupported SetInterface command received: %02X", __PRETTY_FUNCTION__, ctrl->cmd[5])
			break;

		default:
			LOGWARN("%s Unknown SetInterface command received: %02X", __PRETTY_FUNCTION__, ctrl->cmd[5])
			break;
	}
}

void SCSIDaynaPort::SetMcastAddr()
{
	ctrl->length = ctrl->cmd[4];
	if (ctrl->length == 0) {
		LOGWARN("%s Not supported SetMcastAddr Command %02X", __PRETTY_FUNCTION__, ctrl->cmd[2])

		throw scsi_error_exception();
	}

	EnterDataOutPhase();
}

void SCSIDaynaPort::EnableInterface()
{
	if (!EnableInterface(ctrl->cmd)) {
		throw scsi_error_exception();
	}

	EnterStatusPhase();
}

int SCSIDaynaPort::GetSendDelay() const
{
	// The Daynaport needs to have a delay after the size/flags field
	// of the read response. In the MacOS driver, it looks like the
	// driver is doing two "READ" system calls.
	return DAYNAPORT_READ_HEADER_SZ;
}
