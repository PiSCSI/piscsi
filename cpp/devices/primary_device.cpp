//---------------------------------------------------------------------------
//
// SCSI Target Emulator RaSCSI Reloaded
// for Raspberry Pi
//
// Copyright (C) 2022 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "log.h"
#include "rascsi_exceptions.h"
#include "scsi_command_util.h"
#include "primary_device.h"

using namespace std;
using namespace scsi_defs;
using namespace scsi_command_util;

bool PrimaryDevice::Init(const unordered_map<string, string>& params)
{
	// Mandatory SCSI primary commands
	AddCommand(scsi_command::eCmdTestUnitReady, [this] { TestUnitReady(); });
	AddCommand(scsi_command::eCmdInquiry, [this] { Inquiry(); });
	AddCommand(scsi_command::eCmdReportLuns, [this] { ReportLuns(); });

	// Optional commands supported by all RaSCSI devices
	AddCommand(scsi_command::eCmdRequestSense, [this] { RequestSense(); });
	AddCommand(scsi_command::eCmdReserve6, [this] { ReserveUnit(); });
	AddCommand(scsi_command::eCmdRelease6, [this] { ReleaseUnit(); });
	AddCommand(scsi_command::eCmdSendDiagnostic, [this] { SendDiagnostic(); });

	SetParams(params);

	return true;
}

void PrimaryDevice::AddCommand(scsi_command opcode, const operation& execute)
{
	commands[opcode] = execute;
}

void PrimaryDevice::Dispatch(scsi_command cmd)
{
	if (const auto& it = commands.find(cmd); it != commands.end()) {
		LOGDEBUG("Executing %s ($%02X)", command_names.find(cmd)->second, static_cast<int>(cmd))

		it->second();
	}
	else {
		LOGTRACE("ID %d LUN %d received unsupported command: $%02X", GetId(), GetLun(), static_cast<int>(cmd))

		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_COMMAND_OPERATION_CODE);
	}
}

void PrimaryDevice::Reset()
{
	DiscardReservation();

	Device::Reset();
}

int PrimaryDevice::GetId() const
{
	if (GetController() == nullptr) {
		LOGERROR("Device is missing its controller")
	}

	return GetController() != nullptr ? GetController()->GetTargetId() : -1;
}

void PrimaryDevice::SetController(shared_ptr<AbstractController> c)
{
	controller = c;
}

void PrimaryDevice::TestUnitReady()
{
	CheckReady();

	EnterStatusPhase();
}

void PrimaryDevice::Inquiry()
{
	// EVPD and page code check
	if ((GetController()->GetCmd(1) & 0x01) || GetController()->GetCmd(2)) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	vector<uint8_t> buf = InquiryInternal();

	const size_t allocation_length = min(buf.size(), static_cast<size_t>(GetInt16(GetController()->GetCmd(), 3)));

	memcpy(GetController()->GetBuffer().data(), buf.data(), allocation_length);
	GetController()->SetLength(static_cast<uint32_t>(allocation_length));

	// Report if the device does not support the requested LUN
	if (int lun = GetController()->GetEffectiveLun(); !GetController()->HasDeviceForLun(lun)) {
		LOGTRACE("Reporting LUN %d for device ID %d as not supported", lun, GetId())

		// Signal that the requested LUN does not exist
		GetController()->GetBuffer().data()[0] = 0x7f;
	}

	EnterDataInPhase();
}

void PrimaryDevice::ReportLuns()
{
	// Only SELECT REPORT mode 0 is supported
	if (GetController()->GetCmd(2)) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	const uint32_t allocation_length = GetInt32(GetController()->GetCmd(), 6);

	vector<uint8_t>& buf = GetController()->GetBuffer();
	fill_n(buf.begin(), min(buf.size(), static_cast<size_t>(allocation_length)), 0);

	uint32_t size = 0;
	for (int lun = 0; lun < GetController()->GetMaxLuns(); lun++) {
		if (GetController()->HasDeviceForLun(lun)) {
			size += 8;
			buf[size + 7] = (uint8_t)lun;
		}
	}

	SetInt16(buf, 2, size);

	size += 8;

	GetController()->SetLength(min(allocation_length, size));

	EnterDataInPhase();
}

void PrimaryDevice::RequestSense()
{
	int lun = GetController()->GetEffectiveLun();

    // Note: According to the SCSI specs the LUN handling for REQUEST SENSE non-existing LUNs do *not* result
	// in CHECK CONDITION. Only the Sense Key and ASC are set in order to signal the non-existing LUN.
	if (!GetController()->HasDeviceForLun(lun)) {
        // LUN 0 can be assumed to be present (required to call RequestSense() below)
		assert(GetController()->HasDeviceForLun(0));

		lun = 0;

		// Do not raise an exception here because the rest of the code must be executed
		GetController()->Error(sense_key::ILLEGAL_REQUEST, asc::INVALID_LUN);

		GetController()->SetStatus(status::GOOD);
	}

    vector<byte> buf = GetController()->GetDeviceForLun(lun)->HandleRequestSense();

	const size_t allocation_length = min(buf.size(), static_cast<size_t>(GetController()->GetCmd(4)));

    memcpy(GetController()->GetBuffer().data(), buf.data(), allocation_length);
    GetController()->SetLength(static_cast<uint32_t>(allocation_length));

    EnterDataInPhase();
}

void PrimaryDevice::SendDiagnostic()
{
	// Do not support PF bit
	if (GetController()->GetCmd(1) & 0x10) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	// Do not support parameter list
	if ((GetController()->GetCmd(3) != 0) || (GetController()->GetCmd(4) != 0)) {
		throw scsi_exception(sense_key::ILLEGAL_REQUEST, asc::INVALID_FIELD_IN_CDB);
	}

	EnterStatusPhase();
}

void PrimaryDevice::CheckReady()
{
	// Not ready if reset
	if (IsReset()) {
		SetReset(false);
		LOGTRACE("%s Device in reset", __PRETTY_FUNCTION__)
		throw scsi_exception(sense_key::UNIT_ATTENTION, asc::POWER_ON_OR_RESET);
	}

	// Not ready if it needs attention
	if (IsAttn()) {
		SetAttn(false);
		LOGTRACE("%s Device in needs attention", __PRETTY_FUNCTION__)
		throw scsi_exception(sense_key::UNIT_ATTENTION, asc::NOT_READY_TO_READY_CHANGE);
	}

	// Return status if not ready
	if (!IsReady()) {
		LOGTRACE("%s Device not ready", __PRETTY_FUNCTION__)
		throw scsi_exception(sense_key::NOT_READY, asc::MEDIUM_NOT_PRESENT);
	}

	// Initialization with no error
	LOGTRACE("%s Device is ready", __PRETTY_FUNCTION__)
}

vector<uint8_t> PrimaryDevice::HandleInquiry(device_type type, scsi_level level, bool is_removable) const
{
	vector<uint8_t> buf(0x1F + 5);

	// Basic data
	// buf[0] ... SCSI device type
	// buf[1] ... Bit 7: Removable/not removable
	// buf[2] ... SCSI compliance level of command system
	// buf[3] ... SCSI compliance level of Inquiry response
	// buf[4] ... Inquiry additional data
	buf[0] = static_cast<uint8_t>(type);
	buf[1] = is_removable ? 0x80 : 0x00;
	buf[2] = static_cast<uint8_t>(level);
	buf[3] = level >= scsi_level::SCSI_2 ?
			static_cast<uint8_t>(scsi_level::SCSI_2) : static_cast<uint8_t>(scsi_level::SCSI_1_CCS);
	buf[4] = 0x1F;

	// Padded vendor, product, revision
	memcpy(&buf.data()[8], GetPaddedName().c_str(), 28);

	return buf;
}

vector<byte> PrimaryDevice::HandleRequestSense() const
{
	// Return not ready only if there are no errors
	if (!GetStatusCode() && !IsReady()) {
		throw scsi_exception(sense_key::NOT_READY, asc::MEDIUM_NOT_PRESENT);
	}

	// Set 18 bytes including extended sense data

	vector<byte> buf(18);

	// Current error
	buf[0] = (byte)0x70;

	buf[2] = (byte)(GetStatusCode() >> 16);
	buf[7] = (byte)10;
	buf[12] = (byte)(GetStatusCode() >> 8);
	buf[13] = (byte)GetStatusCode();

	LOGTRACE("%s Status $%02X, Sense Key $%02X, ASC $%02X",__PRETTY_FUNCTION__, static_cast<int>(GetController()->GetStatus()),
			static_cast<int>(buf[2]), static_cast<int>(buf[12]))

	return buf;
}

bool PrimaryDevice::WriteByteSequence(vector<uint8_t>&, uint32_t)
{
	LOGERROR("%s Writing bytes is not supported by this device", __PRETTY_FUNCTION__)

	return false;
}

void PrimaryDevice::ReserveUnit()
{
	reserving_initiator = GetController()->GetInitiatorId();

	if (reserving_initiator != -1) {
		LOGTRACE("Reserved device ID %d, LUN %d for initiator ID %d", GetId(), GetLun(), reserving_initiator)
	}
	else {
		LOGTRACE("Reserved device ID %d, LUN %d for unknown initiator", GetId(), GetLun())
	}

	EnterStatusPhase();
}

void PrimaryDevice::ReleaseUnit()
{
	if (reserving_initiator != -1) {
		LOGTRACE("Released device ID %d, LUN %d reserved by initiator ID %d", GetId(), GetLun(), reserving_initiator)
	}
	else {
		LOGTRACE("Released device ID %d, LUN %d reserved by unknown initiator", GetId(), GetLun())
	}

	DiscardReservation();

	EnterStatusPhase();
}

bool PrimaryDevice::CheckReservation(int initiator_id, scsi_command cmd, bool prevent_removal) const
{
	if (reserving_initiator == NOT_RESERVED || reserving_initiator == initiator_id) {
		return true;
	}

	// A reservation is valid for all commands except those excluded below
	if (cmd == scsi_command::eCmdInquiry || cmd == scsi_command::eCmdRequestSense || cmd == scsi_command::eCmdRelease6) {
		return true;
	}
	// PREVENT ALLOW MEDIUM REMOVAL is permitted if the prevent bit is 0
	if (cmd == scsi_command::eCmdPreventAllowMediumRemoval && !prevent_removal) {
		return true;
	}

	if (initiator_id != -1) {
		LOGTRACE("Initiator ID %d tries to access reserved device ID %d, LUN %d", initiator_id, GetId(), GetLun())
	}
	else {
		LOGTRACE("Unknown initiator tries to access reserved device ID %d, LUN %d", GetId(), GetLun())
	}

	return false;
}

void PrimaryDevice::DiscardReservation()
{
	reserving_initiator = NOT_RESERVED;
}
