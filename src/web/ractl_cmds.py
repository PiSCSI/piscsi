import logging

from settings import *
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
    

def validate_scsi_id(scsi_id):
    """
    Checks that scsi_id is a valid SCSI ID, i.e. a number between 0 and 7.
    Returns a dict with:
    - boolean status
    - str msg (result message)
    """
    from re import match
    if match("[0-7]", str(scsi_id)) != None:
        return {"status": True, "msg": "Valid SCSI ID."}
    else:
        return {"status": False, "msg": "Invalid SCSI ID. Should be a number between 0-7"}


def get_valid_scsi_ids(invalid_ids):
    """
    Takes a list of dicts devices, and list of ints reserved_ids.
    Returns a list of ints valid_ids, which are the SCSI ids where it is possible to attach a device.
    """
    valid_ids = list(range(8))
    for id in invalid_ids:
        try:
            valid_ids.remove(int(id))
        except:
            # May reach this state if the RaSCSI Web UI thinks an ID
            # is reserved but RaSCSI has not actually reserved it.
            logging.warning(f"SCSI ID {id} flagged as both valid and invalid. \
                    Try restarting the RaSCSI Web UI.")
    valid_ids.reverse()
    return valid_ids


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
            return {"status": False, "msg": f"Cannot insert an image for \
                    {device_type} into a {current_type} device."}
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
        dun = result.devices_info.devices[n].unit
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

        device_list.append({"id": did, "un": dun, "device_type": dtype, \
                "status": ", ".join(dstat_msg), "image": dpath, "file": dfile, "params": dparam,\
                "vendor": dven, "product": dprod, "revision": drev, "block_size": dblock})
        n += 1

    return {"status": True, "device_list": device_list}


def sort_and_format_devices(devices):
    """
    Takes a list of dicts devices and returns a list of dicts.
    Sorts by SCSI ID acending (0 to 7).
    For SCSI IDs where no device is attached, inject a dict with placeholder text.
    """
    occupied_ids = []
    for d in devices:
        occupied_ids.append(d["id"])

    formatted_devices = devices

    # Add padding devices and sort the list
    for id in range(8):
        if id not in occupied_ids:
            formatted_devices.append({"id": id, "device_type": "-", \
                    "status": "-", "file": "-", "product": "-"})
    # Sort list of devices by id
    formatted_devices.sort(key=lambda dic: str(dic["id"]))

    return formatted_devices


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


def send_pb_command(payload):
    """
    Takes a str containing a serialized protobuf as argument.
    Establishes a socket connection with RaSCSI.
    """
    # Host and port number where rascsi is listening for socket connections
    HOST = 'localhost'
    PORT = 6868

    counter = 0
    tries = 100
    error_msg = ""

    import socket
    while counter < tries:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((HOST, PORT))
                return send_over_socket(s, payload)
        except socket.error as error:
            counter += 1
            logging.warning("The RaSCSI service is not responding - attempt " + \
                    str(counter) + "/" + str(tries))
            error_msg = str(error)

    logging.error(error_msg)

    # After failing all attempts, throw a 404 error
    from flask import abort
    abort(404, "Failed to connect to RaSCSI at " + str(HOST) + ":" + str(PORT) + \
            " with error: " + error_msg + ". Is the RaSCSI service running?")


def send_over_socket(s, payload):
    """
    Takes a socket object and str payload with serialized protobuf.
    Sends payload to RaSCSI over socket and captures the response.
    Tries to extract and interpret the protobuf header to get response size.
    Reads data from socket in 2048 bytes chunks until all data is received.

    """
    from struct import pack, unpack

    # Prepending a little endian 32bit header with the message size
    s.send(pack("<i", len(payload)))
    s.send(payload)

    # Receive the first 4 bytes to get the response header
    response = s.recv(4)
    if len(response) >= 4:
        # Extracting the response header to get the length of the response message
        response_length = unpack("<i", response)[0]
        # Reading in chunks, to handle a case where the response message is very large
        chunks = []
        bytes_recvd = 0
        while bytes_recvd < response_length:
            chunk = s.recv(min(response_length - bytes_recvd, 2048))
            if chunk == b'':
                from flask import abort
                logging.error("Read an empty chunk from the socket. \
                        Socket connection has dropped unexpectedly. \
                        RaSCSI may have has crashed.")
                abort(503, "Lost connection to RaSCSI. \
                        Please go back and try again. \
                        If the issue persists, please report a bug.")
            chunks.append(chunk)
            bytes_recvd = bytes_recvd + len(chunk)
        response_message = b''.join(chunks)
        return response_message
    else:
        from flask import abort
        logging.error("The response from RaSCSI did not contain a protobuf header. \
                RaSCSI may have crashed.")
        abort(500, "Did not get a valid response from RaSCSI. \
                Please go back and try again. \
                If the issue persists, please report a bug.")
