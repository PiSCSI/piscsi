import logging

from settings import *
from socket_cmds import send_pb_command
import rascsi_interface_pb2 as proto


def get_server_info():
    """
    Sends a SERVER_INFO command to the server.
    Returns a dict with:
    - boolean status
    - str version (RaSCSI version number)
    - list of str log_levels (the log levels RaSCSI supports)
    - str current_log_level
    - list of int reserved_ids
    - 5 distinct lists of strs with file endings recognized by RaSCSI
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.SERVER_INFO

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    version = str(result.server_info.version_info.major_version) + "." +\
              str(result.server_info.version_info.minor_version) + "." +\
              str(result.server_info.version_info.patch_version)
    log_levels = result.server_info.log_level_info.log_levels
    current_log_level = result.server_info.log_level_info.current_log_level
    reserved_ids = list(result.server_info.reserved_ids_info.ids)
    image_dir = result.server_info.image_files_info.default_image_folder

    # Creates lists of file endings recognized by RaSCSI
    mappings = result.server_info.mapping_info.mapping
    sahd = []
    schd = []
    scrm = []
    scmo = []
    sccd = []
    for m in mappings:
        if mappings[m] == proto.PbDeviceType.SAHD:
            sahd.append(m)
        elif mappings[m] == proto.PbDeviceType.SCHD:
            schd.append(m)
        elif mappings[m] == proto.PbDeviceType.SCRM:
            scrm.append(m)
        elif mappings[m] == proto.PbDeviceType.SCMO:
            scmo.append(m)
        elif mappings[m] == proto.PbDeviceType.SCCD:
            sccd.append(m)

    return {
            "status": result.status, 
            "version": version, 
            "log_levels": log_levels, 
            "current_log_level": current_log_level, 
            "reserved_ids": reserved_ids,
            "image_dir": image_dir,
            "sahd": sahd,
            "schd": schd,
            "scrm": scrm,
            "scmo": scmo,
            "sccd": sccd,
            }


def get_network_info():
    """
    Sends a NETWORK_INTERFACES_INFO command to the server.
    Returns a dict with:
    - boolean status
    - list of str ifs (network interfaces detected by RaSCSI)
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.NETWORK_INTERFACES_INFO

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    ifs = result.network_interfaces_info.name
    return {"status": result.status, "ifs": ifs}


def get_device_types():
    """
    Sends a DEVICE_TYPES_INFO command to the server.
    Returns a dict with:
    - boolean status
    - list of str device_types (device types that RaSCSI supports, ex. SCHD, SCCD, etc)
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICE_TYPES_INFO

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    device_types = []
    for t in result.device_types_info.properties:
        device_types.append(proto.PbDeviceType.Name(t.type))
    return {"status": result.status, "device_types": device_types}


def attach_image(scsi_id, **kwargs):
    """
    Takes int scsi_id and kwargs containing 0 or more device properties

    If the current attached device is a removable device wihout media inserted,
    this sends a INJECT command to the server.
    If there is no currently attached device, this sends the ATTACH command to the server.

    Returns boolean status and str msg

    """
    command = proto.PbCommand()
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)

    if "device_type" in kwargs.keys():
        if kwargs["device_type"] not in [None, ""]:
            devices.type = proto.PbDeviceType.Value(str(kwargs["device_type"]))
    if "unit" in kwargs.keys():
        if kwargs["unit"] not in [None, ""]:
            devices.unit = kwargs["unit"]
    if "image" in kwargs.keys():
        if kwargs["image"] not in [None, ""]:
            devices.params["file"] = kwargs["image"]

    # Handling the inserting of media into an attached removable type device
    device_type = kwargs.get("device_type", None) 
    currently_attached = list_devices(scsi_id, kwargs.get("unit"))["device_list"]
    if len(currently_attached) > 0:
        current_type = currently_attached[0]["device_type"] 
    else:
        current_type = None

    if device_type in REMOVABLE_DEVICE_TYPES and current_type in REMOVABLE_DEVICE_TYPES:
        if current_type != device_type:
            return {
                    "status": False,
                    "msg": "Cannot insert an image for " + device_type + \
                    " into a " + current_type + " device."
                   }
        else:
            command.operation = proto.PbOperation.INSERT
    # Handling attaching a new device
    else:
        command.operation = proto.PbOperation.ATTACH
        if "interfaces" in kwargs.keys():
            if kwargs["interfaces"] not in [None, ""]:
                devices.params["interfaces"] = kwargs["interfaces"]
        if "vendor" in kwargs.keys():
            if kwargs["vendor"] != None:
                devices.vendor = kwargs["vendor"]
        if "product" in kwargs.keys():
            if kwargs["product"] != None:
                devices.product = kwargs["product"]
        if "revision" in kwargs.keys():
            if kwargs["revision"] != None:
                devices.revision = kwargs["revision"]
        if "block_size" in kwargs.keys():
            if kwargs["block_size"] not in [None, ""]:
                devices.block_size = int(kwargs["block_size"])

    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def detach_by_id(scsi_id, un=None):
    """
    Takes int scsi_id and optional int un
    Sends a DETACH command to the server.
    Returns boolean status and str msg.
    """
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    if un != None:
        devices.unit = int(un)

    command = proto.PbCommand()
    command.operation = proto.PbOperation.DETACH
    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def detach_all():
    """
    Sends a DETACH_ALL command to the server.
    Returns boolean status and str msg.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DETACH_ALL

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def eject_by_id(scsi_id, un=None):
    """
    Takes int scsi_id and optional int un.
    Sends an EJECT command to the server.
    Returns boolean status and str msg.
    """
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    if un != None:
        devices.unit = int(un)

    command = proto.PbCommand()
    command.operation = proto.PbOperation.EJECT
    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def list_devices(scsi_id=None, un=None):
    """
    Takes optional int scsi_id and optional int un.
    Sends a DEVICES_INFO command to the server.
    If no scsi_id is provided, returns a list of dicts of all attached devices.
    If scsi_id is is provided, returns a list of one dict for the given device.
    If no attached device is found, returns an empty list.
    Returns boolean status, list of dicts device_list
    """
    from os import path
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICES_INFO

    # If method is called with scsi_id parameter, return the info on those devices
    # Otherwise, return the info on all attached devices
    if scsi_id != None:
        device = proto.PbDeviceDefinition()
        device.id = int(scsi_id)
        if un != None:
            device.unit = int(un)
        command.devices.append(device)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)

    device_list = []
    n = 0

    # Return an empty list if no devices are attached
    if len(result.devices_info.devices) == 0:
        return {"status": False, "device_list": []}

    while n < len(result.devices_info.devices):
        did = result.devices_info.devices[n].id
        dunit = result.devices_info.devices[n].unit
        dtype = proto.PbDeviceType.Name(result.devices_info.devices[n].type) 
        dstat = result.devices_info.devices[n].status
        dprop = result.devices_info.devices[n].properties

        # Building the status string
        # TODO: This formatting should probably be moved elsewhere
        dstat_msg = []
        if dprop.read_only == True:
            dstat_msg.append("Read-Only")
        if dstat.protected == True and dprop.protectable == True:
            dstat_msg.append("Write-Protected")
        if dstat.removed == True and dprop.removable == True:
            dstat_msg.append("No Media")
        if dstat.locked == True and dprop.lockable == True:
            dstat_msg.append("Locked")

        dpath = result.devices_info.devices[n].file.name
        dfile = path.basename(dpath)
        dparam = result.devices_info.devices[n].params
        dven = result.devices_info.devices[n].vendor
        dprod = result.devices_info.devices[n].product
        drev = result.devices_info.devices[n].revision
        dblock = result.devices_info.devices[n].block_size
        dsize = int(result.devices_info.devices[n].block_count) * int(dblock)

        device_list.append(
                {
                    "id": did,
                    "un": dunit,
                    "device_type": dtype,
                    "status": ", ".join(dstat_msg),
                    "image": dpath,
                    "file": dfile,
                    "params": dparam,
                    "vendor": dven,
                    "product": dprod,
                    "revision": drev,
                    "block_size": dblock,
                    "size": dsize,
                }
            )
        n += 1

    return {"status": True, "device_list": device_list}


def set_log_level(log_level):
    """
    Sends a LOG_LEVEL command to the server.
    Takes str log_level as an argument.
    Returns boolean status and str msg.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.LOG_LEVEL
    command.params["level"] = str(log_level)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}
