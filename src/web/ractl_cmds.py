import logging

from settings import *
import rascsi_interface_pb2 as proto


def get_server_info():
    command = proto.PbCommand()
    command.operation = proto.PbOperation.SERVER_INFO

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    version = str(result.server_info.major_version) + "." +\
              str(result.server_info.minor_version) + "." +\
              str(result.server_info.patch_version)
    log_levels = result.server_info.log_levels
    current_log_level = result.server_info.current_log_level
    reserved_ids = list(result.server_info.reserved_ids)
    return {"status": result.status, "version": version, "log_levels": log_levels, "current_log_level": current_log_level, "reserved_ids": reserved_ids}
    

def validate_scsi_id(scsi_id):
    from re import match
    if match("[0-7]", str(scsi_id)) != None:
        return {"status": True, "msg": "Valid SCSI ID."}
    else:
        return {"status": False, "msg": "Invalid SCSI ID. Should be a number between 0-7"}


def get_valid_scsi_ids(devices, reserved_ids):
    occupied_ids = []
    for d in devices:
        # Make it possible to insert images on top of a 
        # removable media device currently without an image attached
        if d["device_type"] != "-" and "No Media" not in d["status"]:
            occupied_ids.append(d["id"])

    # Combine lists and remove duplicates
    invalid_ids = list(set(reserved_ids + occupied_ids))
    valid_ids = list(range(8))
    for id in invalid_ids:
        try:
            valid_ids.remove(int(id))
        except:
            # May reach this state if the RaSCSI Web UI thinks an ID
            # is reserved but RaSCSI has not actually reserved it.
            logging.warning(f"SCSI ID {id} flagged as both valid and invalid. Try restarting the RaSCSI Web UI.")
    valid_ids.reverse()
    return valid_ids


def get_type(scsi_id):
    device = proto.PbDeviceDefinition()
    device.id = int(scsi_id)

    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICE_INFO
    command.devices.append(device)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    # Assuming that only one PbDevice object is present in the response
    try:
        result_type = proto.PbDeviceType.Name(result.device_info.devices[0].type)
        return {"status": result.status, "msg": result.msg, "device_type": result_type}
    except:
        return {"status": result.status, "msg": result.msg, "device_type": None}


def attach_image(scsi_id, **kwargs):

    # Handling the inserting of media into an attached removable type device
    currently_attached = get_type(scsi_id)["device_type"] 
    device_type = kwargs.get("device_type", None) 

    if device_type in REMOVABLE_DEVICE_TYPES and currently_attached in REMOVABLE_DEVICE_TYPES:
        if currently_attached != device_type:
            return {"status": False, "msg": f"Cannot insert an image for {device_type} into a {currently_attached} device."}
        else:
            return insert(scsi_id, kwargs.get("image", ""))
    # Handling attaching a new device
    else:
        devices = proto.PbDeviceDefinition()
        devices.id = int(scsi_id)
        if "device_type" in kwargs.keys():
            devices.type = proto.PbDeviceType.Value(str(kwargs["device_type"]))
        if "unit" in kwargs.keys():
            devices.unit = kwargs["unit"]
        if "image" in kwargs.keys():
            if kwargs["image"] not in [None, ""]:
                devices.params["file"] = kwargs["image"]
        if "interfaces" in kwargs.keys():
            if kwargs["interfaces"] not in [None, ""]:
                devices.params["interfaces"] = kwargs["interfaces"]
        if "vendor" in kwargs.keys():
            if kwargs["vendor"] not in [None, ""]:
                devices.vendor = kwargs["vendor"]
        if "product" in kwargs.keys():
            if kwargs["product"] not in [None, ""]:
                devices.product = kwargs["product"]
        if "revision" in kwargs.keys():
            if kwargs["revision"] not in [None, ""]:
                devices.revision = kwargs["revision"]
        if "block_size" in kwargs.keys():
            if kwargs["block_size"] not in [None, ""]:
                devices.block_size = int(kwargs["block_size"])

        command = proto.PbCommand()
        command.operation = proto.PbOperation.ATTACH
        command.devices.append(devices)

        data = send_pb_command(command.SerializeToString())
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}


def detach_by_id(scsi_id):
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)

    command = proto.PbCommand()
    command.operation = proto.PbOperation.DETACH
    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def detach_all():
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DETACH_ALL

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def eject_by_id(scsi_id):
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)

    command = proto.PbCommand()
    command.operation = proto.PbOperation.EJECT
    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def insert(scsi_id, image):
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    devices.params["file"] = image

    command = proto.PbCommand()
    command.operation = proto.PbOperation.INSERT
    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def attach_daynaport(scsi_id, interfaces):
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    devices.type = proto.PbDeviceType.SCDP
    devices.params["interfaces"] = interfaces

    command = proto.PbCommand()
    command.operation = proto.PbOperation.ATTACH
    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def list_devices(scsi_id=None):
    from os import path
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICE_INFO

    # If method is called with scsi_id parameter, return the info on those devices
    # Otherwise, return the info on all attached devices
    if scsi_id != None:
        device = proto.PbDeviceDefinition()
        device.id = int(scsi_id)
        command.devices.append(device)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)

    device_list = []
    n = 0

    if len(result.device_info.devices) == 0:
        return {"status": False, "device_list": []}

    while n < len(result.device_info.devices):
        did = result.device_info.devices[n].id
        dun = result.device_info.devices[n].unit
        dtype = proto.PbDeviceType.Name(result.device_info.devices[n].type) 
        dstat = result.device_info.devices[n].status
        dprop = result.device_info.devices[n].properties

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

        dpath = result.device_info.devices[n].file.name
        dfile = path.basename(dpath)
        dparam = result.device_info.devices[n].params
        dven = result.device_info.devices[n].vendor
        dprod = result.device_info.devices[n].product
        drev = result.device_info.devices[n].revision
        dblock = result.device_info.devices[n].block_size

        device_list.append({"id": did, "un": dun, "device_type": dtype, \
                "status": ", ".join(dstat_msg), "image": dpath, "file": dfile, "params": dparam,\
                "vendor": dven, "product": dprod, "revision": drev, "block_size": dblock})
        n += 1

    return {"status": True, "device_list": device_list}


def sort_and_format_devices(devices):
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


def reserve_scsi_ids(reserved_scsi_ids):
    '''Sends a command to the server to reserve SCSI IDs. Takes a list of strings as argument.'''
    command = proto.PbCommand()
    command.operation = proto.PbOperation.RESERVE
    command.params["ids"] = ",".join(reserved_scsi_ids)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def set_log_level(log_level):
    '''Sends a command to the server to change the log level. Takes target log level as an argument.'''
    command = proto.PbCommand()
    command.operation = proto.PbOperation.LOG_LEVEL
    command.params["level"] = str(log_level)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def send_pb_command(payload):
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
                logging.error("Read an empty chunk from the socket. Socket connection may have dropped unexpectedly.")
                abort(503, "Lost connection to RaSCSI. Please go back and try again. If the issue persists, please report a bug.")
            chunks.append(chunk)
            bytes_recvd = bytes_recvd + len(chunk)
        response_message = b''.join(chunks)
        return response_message
    else:
        from flask import abort
        logging.error("The response from RaSCSI did not contain a protobuf header.")
        abort(500, "Did not get a valid response from RaSCSI. Please go back and try again. If the issue persists, please report a bug.")
