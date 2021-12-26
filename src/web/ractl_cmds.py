"""
Module for commands sent to the RaSCSI backend service.
"""

from settings import REMOVABLE_DEVICE_TYPES
from socket_cmds import send_pb_command
from flask import current_app
from flask_babel import _
import rascsi_interface_pb2 as proto


def get_server_info():
    """
    Sends a SERVER_INFO command to the server.
    Returns a dict with:
    - (bool) status
    - (str) version (RaSCSI version number)
    - (list) of (str) log_levels (the log levels RaSCSI supports)
    - (str) current_log_level
    - (list) of (int) reserved_ids
    - (str) image_dir, path to the default images directory
    - (int) scan_depth, the current images directory scan depth
    - 5 distinct (list)s of (str)s with file endings recognized by RaSCSI
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.SERVER_INFO
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

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
    scan_depth = result.server_info.image_files_info.depth

    # Creates lists of file endings recognized by RaSCSI
    mappings = result.server_info.mapping_info.mapping
    sahd = []
    schd = []
    scrm = []
    scmo = []
    sccd = []
    for dtype in mappings:
        if mappings[dtype] == proto.PbDeviceType.SAHD:
            sahd.append(dtype)
        elif mappings[dtype] == proto.PbDeviceType.SCHD:
            schd.append(dtype)
        elif mappings[dtype] == proto.PbDeviceType.SCRM:
            scrm.append(dtype)
        elif mappings[dtype] == proto.PbDeviceType.SCMO:
            scmo.append(dtype)
        elif mappings[dtype] == proto.PbDeviceType.SCCD:
            sccd.append(dtype)

    return {
        "status": result.status,
        "version": version,
        "log_levels": log_levels,
        "current_log_level": current_log_level,
        "reserved_ids": reserved_ids,
        "image_dir": image_dir,
        "scan_depth": scan_depth,
        "sahd": sahd,
        "schd": schd,
        "scrm": scrm,
        "scmo": scmo,
        "sccd": sccd,
        }


def get_reserved_ids():
    """
    Sends a RESERVED_IDS_INFO command to the server.
    Returns a dict with:
    - (bool) status
    - (list) of (int) ids -- currently reserved SCSI IDs
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.RESERVED_IDS_INFO
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    scsi_ids = []
    for scsi_id in result.reserved_ids_info.ids:
        scsi_ids.append(str(scsi_id))

    return {"status": result.status, "ids": scsi_ids}


def get_network_info():
    """
    Sends a NETWORK_INTERFACES_INFO command to the server.
    Returns a dict with:
    - (bool) status
    - (list) of (str) ifs (network interfaces detected by RaSCSI)
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.NETWORK_INTERFACES_INFO
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    ifs = result.network_interfaces_info.name
    return {"status": result.status, "ifs": ifs}


def get_device_types():
    """
    Sends a DEVICE_TYPES_INFO command to the server.
    Returns a dict with:
    - (bool) status
    - (list) of (str) device_types (device types that RaSCSI supports, ex. SCHD, SCCD, etc)
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICE_TYPES_INFO
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    device_types = []
    for prop in result.device_types_info.properties:
        device_types.append(proto.PbDeviceType.Name(prop.type))
    return {"status": result.status, "device_types": device_types}


def get_image_files_info():
    """
    Sends a DEFAULT_IMAGE_FILES_INFO command to the server.
    Returns a dict with:
    - (bool) status
    - (str) images_dir, path to images dir
    - (list) of (str) image_files
    - (int) scan_depth, the current scan depth
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEFAULT_IMAGE_FILES_INFO
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    images_dir = result.image_files_info.default_image_folder
    image_files = result.image_files_info.image_files
    scan_depth = result.image_files_info.depth
    return {
        "status": result.status,
        "images_dir": images_dir,
        "image_files": image_files,
        "scan_depth": scan_depth,
        }


def attach_image(scsi_id, **kwargs):
    """
    Takes (int) scsi_id and kwargs containing 0 or more device properties

    If the current attached device is a removable device wihout media inserted,
    this sends a INJECT command to the server.
    If there is no currently attached device, this sends the ATTACH command to the server.

    Returns (bool) status and (str) msg

    """
    command = proto.PbCommand()
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]
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
    if currently_attached:
        current_type = currently_attached[0]["device_type"]
    else:
        current_type = None

    if device_type in REMOVABLE_DEVICE_TYPES and current_type in REMOVABLE_DEVICE_TYPES:
        if current_type != device_type:
            return {
                "status": False,
                "msg": _(
                    u"Cannot insert an image for %(device_type)s into a "
                    u"%(current_device_type)s device",
                    device_type=device_type,
                    current_device_type=current_type
                    ),
                }
        command.operation = proto.PbOperation.INSERT
    # Handling attaching a new device
    else:
        command.operation = proto.PbOperation.ATTACH
        if "interfaces" in kwargs.keys():
            if kwargs["interfaces"] not in [None, ""]:
                devices.params["interfaces"] = kwargs["interfaces"]
        if "vendor" in kwargs.keys():
            if kwargs["vendor"] is not None:
                devices.vendor = kwargs["vendor"]
        if "product" in kwargs.keys():
            if kwargs["product"] is not None:
                devices.product = kwargs["product"]
        if "revision" in kwargs.keys():
            if kwargs["revision"] is not None:
                devices.revision = kwargs["revision"]
        if "block_size" in kwargs.keys():
            if kwargs["block_size"] not in [None, ""]:
                devices.block_size = int(kwargs["block_size"])

    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def detach_by_id(scsi_id, unit=None):
    """
    Takes (int) scsi_id and optional (int) unit.
    Sends a DETACH command to the server.
    Returns (bool) status and (str) msg.
    """
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    if unit is not None:
        devices.unit = int(unit)

    command = proto.PbCommand()
    command.operation = proto.PbOperation.DETACH
    command.devices.append(devices)
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def detach_all():
    """
    Sends a DETACH_ALL command to the server.
    Returns (bool) status and (str) msg.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DETACH_ALL
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def eject_by_id(scsi_id, unit=None):
    """
    Takes (int) scsi_id and optional (int) unit.
    Sends an EJECT command to the server.
    Returns (bool) status and (str) msg.
    """
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    if unit is not None:
        devices.unit = int(unit)

    command = proto.PbCommand()
    command.operation = proto.PbOperation.EJECT
    command.devices.append(devices)
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def list_devices(scsi_id=None, unit=None):
    """
    Takes optional (int) scsi_id and optional (int) unit.
    Sends a DEVICES_INFO command to the server.
    If no scsi_id is provided, returns a (list) of (dict)s of all attached devices.
    If scsi_id is is provided, returns a (list) of one (dict) for the given device.
    If no attached device is found, returns an empty (list).
    Returns (bool) status, (list) of dicts device_list
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICES_INFO
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    # If method is called with scsi_id parameter, return the info on those devices
    # Otherwise, return the info on all attached devices
    if scsi_id is not None:
        device = proto.PbDeviceDefinition()
        device.id = int(scsi_id)
        if unit is not None:
            device.unit = int(unit)
        command.devices.append(device)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)

    device_list = []

    # Return an empty (list) if no devices are attached
    if not result.devices_info.devices:
        return {"status": False, "device_list": []}

    image_files_info = get_image_files_info()
    i = 0
    while i < len(result.devices_info.devices):
        did = result.devices_info.devices[i].id
        dunit = result.devices_info.devices[i].unit
        dtype = proto.PbDeviceType.Name(result.devices_info.devices[i].type)
        dstat = result.devices_info.devices[i].status
        dprop = result.devices_info.devices[i].properties

        # Building the status string
        dstat_msg = []
        if dprop.read_only:
            dstat_msg.append("Read-Only")
        if dstat.protected and dprop.protectable:
            dstat_msg.append("Write-Protected")
        if dstat.removed and dprop.removable:
            dstat_msg.append("No Media")
        if dstat.locked and dprop.lockable:
            dstat_msg.append("Locked")

        dpath = result.devices_info.devices[i].file.name
        dfile = dpath.replace(image_files_info["images_dir"] + "/", "")
        dparam = result.devices_info.devices[i].params
        dven = result.devices_info.devices[i].vendor
        dprod = result.devices_info.devices[i].product
        drev = result.devices_info.devices[i].revision
        dblock = result.devices_info.devices[i].block_size
        dsize = int(result.devices_info.devices[i].block_count) * int(dblock)

        device_list.append({
            "id": did,
            "unit": dunit,
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
            })
        i += 1

    return {"status": result.status, "msg": result.msg, "device_list": device_list}


def reserve_scsi_ids(reserved_scsi_ids):
    """
    Sends the RESERVE_IDS command to the server to reserve SCSI IDs.
    Takes a (list) of (str) as argument.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.RESERVE_IDS
    command.params["ids"] = ",".join(reserved_scsi_ids)
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def set_log_level(log_level):
    """
    Sends a LOG_LEVEL command to the server.
    Takes (str) log_level as an argument.
    Returns (bool) status and (str) msg.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.LOG_LEVEL
    command.params["level"] = str(log_level)
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def shutdown_pi(mode):
    """
    Sends a SHUT_DOWN command to the server.
    Takes (str) mode as an argument.
    Returns (bool) status and (str) msg.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.SHUT_DOWN
    command.params["mode"] = str(mode)
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def is_token_auth():
    """
    Sends a CHECK_AUTHENTICATION command to the server.
    Tells you whether RaSCSI backend is protected by a token password or not.
    Returns (bool) status and (str) msg.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.CHECK_AUTHENTICATION
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}
