//---------------------------------------------------------------------------
//
//	SCSI Target Emulator PiSCSI
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
//    - https://github.com/PiSCSI/piscsi/wiki/Dayna-Port-SCSI-Link
//
//  Note: This requires a DaynaPort SCSI Link driver.
//---------------------------------------------------------------------------

#include "shared/piscsi_exceptions.h"
#include "scsi_command_util.h"
#include "scsi_daynaport.h"
#include <sstream>
#include <iomanip>

using namespace scsi_defs;
using namespace scsi_command_util;

SCSIDaynaPort::SCSIDaynaPort(int lun) : PrimaryDevice(SCDP, lun)
{
	SupportsParams(true);
}

bool SCSIDaynaPort::Init(const param_map& params)
{
	PrimaryDevice::Init(params);

	AddCommand(scsi_command::eCmdTestUnitReady, [this] { TestUnitReady(); });
	AddCommand(scsi_command::eCmdRead6, [this] { Read6(); });
	AddCommand(scsi_command::eCmdWrite6, [this] { Write6(); });
	AddCommand(scsi_command::eCmdRetrieveStats, [this] { RetrieveStatistics(); });
	AddCommand(scsi_command::eCmdSetIfaceMode, [this] { SetInterfaceMode(); });
	AddCommand(scsi_command::eCmdSetMcastAddr, [this] { SetMcastAddr(); });
	AddCommand(scsi_command::eCmdEnableInterface, [this] { EnableInterface(); });

	// The Daynaport needs to have a delay after the size/flags field of the read response.
	// In the MacOS driver, it looks like the driver is doing two "READ" system calls.
	SetSendDelay(DAYNAPORT_READ_HEADER_SZ);

	tap_enabled = tap.Init(GetParams());
	if (!tap_enabled) {
// Not terminating on regular Linux PCs is helpful for testing
#if !defined(__x86_64__) && !defined(__X86__)
		return false;
#endif
	} else {
		LogTrace("Tap interface created");
	}

	Reset();
	SetReady(true);
	SetReset(false);

	return true;
}

void SCSIDaynaPort::CleanUp()
{
	tap.CleanUp();
}

vector<uint8_t> SCSIDaynaPort::InquiryInternal() const
{
	vector<uint8_t> buf = HandleInquiry(device_type::processor, scsi_level::scsi_2, false);

	// The Daynaport driver for the Mac expects 37 bytes: Increase additional length and
	// add a vendor-specific byte in order to satisfy this driver.
	buf[4]++;
	buf.push_back(0);

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
int SCSIDaynaPort::Read(cdb_t cdb, vector<uint8_t>& buf, uint64_t)
{
	int rx_packet_size = 0;
	const auto response = (scsi_resp_read_t*)buf.data();

	const int requested_length = cdb[4];

	LogTrace("Read maximum length: " + to_string(requested_length));

	// At startup the host may send a READ(6) command with a sector count of 1 to read the root sector.
	// We should respond by going into the status mode with a code of 0x02.
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
		rx_packet_size = tap.Receive(&buf[DAYNAPORT_READ_HEADER_SZ]);

		// If we didn't receive anything, return size of 0
		if (rx_packet_size <= 0) {
			LogTrace("No packet received");
			response->length = 0;
			response->flags = read_data_flags_t::e_no_more_data;
			return DAYNAPORT_READ_HEADER_SZ;
		}

        byte_read_count += rx_packet_size;

		LogTrace("Packet Size " + to_string(rx_packet_size) + ", read count: " + to_string(read_count));

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
			stringstream s;
			s << "Received a packet that's not for me:" << setfill('0') << setw(2) << hex;
			for (int i = 0 ; i < 6; i++) {
				s << " $" << static_cast<int>(response->data[i]);
			}
			LogDebug(s.str());

			// If there are pending packets to be processed, we'll tell the host that the read
			// length was 0.
			if (!tap.HasPendingPackets()) {
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
				// A frame must have at least 64 bytes (see https://github.com/PiSCSI/piscsi/issues/619)
				// Note that this work-around breaks the checksum. As currently there are no known drivers
				// that care for the checksum, and the Daynaport driver for the Atari expects frames of
				// 64 bytes it was decided to accept the broken checksum. If a driver should pop up that
				// breaks because of this, the work-around has to be re-evaluated.
				size = 64;
			}
			SetInt16(buf, 0, size);
			SetInt32(buf, 2, tap.HasPendingPackets() ? 0x10 : 0x00);

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
bool SCSIDaynaPort::Write(cdb_t cdb, span<const uint8_t> buf)
{
	if (const int data_format = cdb[5]; data_format == 0x00) {
		const int data_length = GetInt16(cdb, 3);
		tap.Send(buf.data(), data_length);
		byte_write_count += data_length;
		LogTrace("Transmitted " + to_string(data_length) + " byte(s) (00 format)");
	}
	else if (data_format == 0x80) {
		// The data length is specified in the first 2 bytes of the payload
		const int data_length = buf[1] + ((static_cast<int>(buf[0]) & 0xff) << 8);
		tap.Send(&(buf.data()[4]), data_length);
		byte_write_count += data_length;
		LogTrace("Transmitted " + to_string(data_length) + "byte(s) (80 format)");
	}
	else {
		stringstream s;
		s << "Unknown data format: " << setfill('0') << setw(2) << hex << data_format;
		LogWarn(s.str());
	}

	GetController()->SetBlocks(0);

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
int SCSIDaynaPort::RetrieveStats(cdb_t cdb, vector<uint8_t>& buf) const
{
	memcpy(buf.data(), &m_scsi_link_stats, sizeof(m_scsi_link_stats));

	return static_cast<int>(min(sizeof(m_scsi_link_stats), static_cast<size_t>(GetInt16(cdb, 3))));
}

void SCSIDaynaPort::TestUnitReady()
{
	// Always successful
	EnterStatusPhase();
}

void SCSIDaynaPort::Read6()
{
	// Get record number and block number
    const uint32_t record = GetInt24(GetController()->GetCmd(), 1) & 0x1fffff;
    GetController()->SetBlocks(1);

	// If any commands have a bogus control value, they were probably not
	// generated by the DaynaPort driver so ignore them
	if (GetController()->GetCmdByte(5) != 0xc0 && GetController()->GetCmdByte(5) != 0x80) {
		LogTrace("Control value: " + to_string(GetController()->GetCmdByte(5)));
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	stringstream s;
	s << "READ(6) command, record: $" << setfill('0') << setw(8) << hex << record;
	LogTrace(s.str());

	GetController()->SetLength(Read(GetController()->GetCmd(), GetController()->GetBuffer(), record));
	LogTrace("Length is " + to_string(GetController()->GetLength()));

	// Set next block
	GetController()->SetNext(record + 1);

	EnterDataInPhase();
}

void SCSIDaynaPort::Write6() const
{
	// Ensure a sufficient buffer size (because it is not transfer for each block)
	GetController()->AllocateBuffer(DAYNAPORT_BUFFER_SIZE);

	const int data_format = GetController()->GetCmdByte(5);

	if (data_format == 0x00) {
		GetController()->SetLength(GetInt16(GetController()->GetCmd(), 3));
	}
	else if (data_format == 0x80) {
		GetController()->SetLength(GetInt16(GetController()->GetCmd(), 3) + 8);
	}
	else {
		stringstream s;
		s << "Unknown data format: " << setfill('0') << setw(2) << hex << data_format;
		LogWarn(s.str());
	}

	stringstream s;
	s << "Length: " << GetController()->GetLength() << ", format: $" << setfill('0') << setw(2) << hex << data_format;
	LogTrace(s.str());

	if (GetController()->GetLength() <= 0) {
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	// Set next block
	GetController()->SetBlocks(1);
	GetController()->SetNext(1);

	EnterDataOutPhase();
}

void SCSIDaynaPort::RetrieveStatistics() const
{
	GetController()->SetLength(RetrieveStats(GetController()->GetCmd(), GetController()->GetBuffer()));

	// Set next block
	GetController()->SetBlocks(1);
	GetController()->SetNext(1);

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
void SCSIDaynaPort::SetInterfaceMode() const
{
	// Check whether this command is telling us to "Set Interface Mode" or "Set MAC Address"

	GetController()->SetLength(RetrieveStats(GetController()->GetCmd(), GetController()->GetBuffer()));
	switch(GetController()->GetCmdByte(5)){
		case CMD_SCSILINK_SETMODE:
			// Not implemented, do nothing
			EnterStatusPhase();
			break;

		case CMD_SCSILINK_SETMAC:
			GetController()->SetLength(6);
			EnterDataOutPhase();
			break;

		default:
			stringstream s;
			s << "Unsupported SetInterface command: " << setfill('0') << setw(2) << hex << GetController()->GetCmdByte(5);
			LogWarn(s.str());
			throw scsi_exception(sense_key::illegal_request, asc::invalid_command_operation_code);
			break;
	}
}

void SCSIDaynaPort::SetMcastAddr() const
{
	GetController()->SetLength(GetController()->GetCmdByte(4));
	if (GetController()->GetLength() == 0) {
		stringstream s;
		s << "Unsupported SetMcastAddr command: " << setfill('0') << setw(2) << hex << GetController()->GetCmdByte(2);
		LogWarn(s.str());
		throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
	}

	EnterDataOutPhase();
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
void SCSIDaynaPort::EnableInterface() const
{
	if (GetController()->GetCmdByte(5) & 0x80) {
		if (const string error = tap.IpLink(true); !error.empty()) {
			LogWarn("Unable to enable the DaynaPort Interface: " + error);

			throw scsi_exception(sense_key::aborted_command);
		}

		tap.Flush();

		LogInfo("The DaynaPort interface has been ENABLED");
	}
	else {
		if (const string error = tap.IpLink(false); !error.empty()) {
			LogWarn("Unable to disable the DaynaPort Interface: " + error);

			throw scsi_exception(sense_key::aborted_command);
		}

		LogInfo("The DaynaPort interface has been DISABLED");
	}

	EnterStatusPhase();
}

vector<PbStatistics> SCSIDaynaPort::GetStatistics() const
{
	vector<PbStatistics> statistics = PrimaryDevice::GetStatistics();

	PbStatistics s;
	s.set_id(GetId());
	s.set_unit(GetLun());

	s.set_category(PbStatisticsCategory::CATEGORY_INFO);

	s.set_key(BYTE_READ_COUNT);
	s.set_value(byte_read_count);
	statistics.push_back(s);

	s.set_key(BYTE_WRITE_COUNT);
	s.set_value(byte_write_count);
	statistics.push_back(s);

	return statistics;
}
