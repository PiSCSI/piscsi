"""
Module for commands sent to the RaSCSI backend service.
"""
from os import path
from unidecode import unidecode
from socket_cmds import send_pb_command
import rascsi_interface_pb2 as proto

def device_list(token):
    """
    Sends a DEVICES_INFO command to the server.
    Returns a list of dicts with info on all attached devices.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICES_INFO
    command.params["token"] = token
    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)

    dlist = []
    i = 0

    while i < len(result.devices_info.devices):
        did = result.devices_info.devices[i].id
        dtype = proto.PbDeviceType.Name(result.devices_info.devices[i].type)
        dstat = result.devices_info.devices[i].status
        dprop = result.devices_info.devices[i].properties

        # Building the status string
        dstat_msg = []
        if dstat.protected and dprop.protectable:
            dstat_msg.append("Write-Protected")
        if dstat.removed and dprop.removable:
            dstat_msg.append("No Media")
        if dstat.locked and dprop.lockable:
            dstat_msg.append("Locked")

        # Transliterate non-ASCII chars in the file name to ASCII
        dfile = unidecode(path.basename(result.devices_info.devices[i].file.name))
        dven = result.devices_info.devices[i].vendor
        dprod = result.devices_info.devices[i].product

        dlist.append({
            "id": did,
            "device_type": dtype,
            "status": ", ".join(dstat_msg),
            "file": dfile,
            "vendor": dven,
            "product": dprod,
            })
        i += 1

    return dlist


def is_token_auth(token):
    """
    Sends a CHECK_AUTHENTICATION command to the server.
    Tells you whether RaSCSI backend is protected by a token password or not.
    Returns (bool) status and (str) msg.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.CHECK_AUTHENTICATION
    command.params["token"] = token

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}
