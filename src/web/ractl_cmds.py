import logging

from settings import *
import rascsi_interface_pb2 as proto

from fnmatch import translate
valid_file_suffix = ["*.hda", "*.hdn", "*.hdi", "*.nhd", "*.hdf", "*.hds", \
        "*.hdr", "*.iso", "*.cdr", "*.toast", "*.img", "*.zip"]
valid_file_types = r"|".join([translate(x) for x in valid_file_suffix])


def rascsi_version():
    command = proto.PbCommand()
    command.operation = proto.PbOperation.SERVER_INFO

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    version = str(result.server_info.major_version) + "." +\
              str(result.server_info.minor_version) + "." +\
              str(result.server_info.patch_version)
    return {"status": result.status, "msg": version}
    

def list_files():
    from re import match

    files_list = []
    for path, dirs, files in os.walk(base_dir):
        # Only list valid file types
        files = [f for f in files if match(valid_file_types, f)]
        files_list.extend(
            [
                (
                    os.path.join(path, file),
                    # TODO: move formatting to template
                    "{:,.0f}".format(
                        os.path.getsize(os.path.join(path, file)) / float(1 << 20)
                    )
                    + " MB",
                )
                for file in files
            ]
        )
    return files_list


def list_config_files():
    files_list = []
    for root, dirs, files in os.walk(base_dir):
        for file in files:
            if file.endswith(".csv"):
                files_list.append(file)
    return files_list


def validate_scsi_id(scsi_id):
    from re import match
    if match("[0-7]", str(scsi_id)) != None:
        return {"status": True, "msg": "Valid SCSI ID."}
    else:
        return {"status": False, "msg": "Invalid SCSI ID. Should be a number between 0-7"}


def get_valid_scsi_ids(devices, invalid_list, occupied_ids):
    for device in devices:
        # Make it possible to insert images on top of a 
        # removable media device currently without an image attached
        if "No Media" in device["status"]:
            occupied_ids.remove(device["id"])

    # Combine lists and remove duplicates
    invalid_ids = list(set(invalid_list + occupied_ids))
    valid_ids = list(range(8))
    for id in invalid_ids:
        valid_ids.remove(int(id))
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
        return {"status": result.status, "msg": result.msg, "type": result_type}
    except:
        return {"status": result.status, "msg": result.msg, "type": ""}


def attach_image(scsi_id, image, image_type):
    if image_type == "SCCD" and get_type(scsi_id)["type"] == "SCCD":
        return insert(scsi_id, image)
    else:
        devices = proto.PbDeviceDefinition()
        devices.id = int(scsi_id)
        devices.type = proto.PbDeviceType.Value(image_type)
        devices.params.append(image)

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
    devices.params.append(image)

    command = proto.PbCommand()
    command.operation = proto.PbOperation.INSERT
    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def attach_daynaport(scsi_id):
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    devices.type = proto.PbDeviceType.SCDP

    command = proto.PbCommand()
    command.operation = proto.PbOperation.ATTACH
    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def is_bridge_setup():
    from subprocess import run
    process = run(["brctl", "show"], capture_output=True)
    output = process.stdout.decode("utf-8")
    if "rascsi_bridge" in output:
        return True
    return False


def rascsi_service(action):
    # start/stop/restart
    from subprocess import run
    return (
        run(["sudo", "/bin/systemctl", action, "rascsi.service"]).returncode
        == 0
    )


def list_devices():
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICE_INFO

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)

    device_list = []
    occupied_ids = []
    n = 0
    while n < len(result.device_info.devices):
        did = result.device_info.devices[n].id
        dun = result.device_info.devices[n].unit
        dtype = proto.PbDeviceType.Name(result.device_info.devices[n].type) 
        dstat = result.device_info.devices[n].status
        dprop = result.device_info.devices[n].properties

        # Building the status string, starting with 'Attached' for every active device
        dstat_msg = ["Attached"]
        if dprop.read_only == True:
            dstat_msg.append("Read-Only")
        if dstat.protected == True and dprop.protectable == True:
            dstat_msg.append("Write-Protected")
        if dstat.removed == True and dprop.removable == True:
            dstat_msg.append("No Media")
        if dstat.locked == True and dprop.lockable == True:
            dstat_msg.append("Locked")

        dfile = result.device_info.devices[n].file.name or "-"
        dprod = result.device_info.devices[n].vendor + " " \
                + result.device_info.devices[n].product
        dblock = result.device_info.devices[n].block_size or "N/A"

        device_list.append({"id": str(did), "un": str(dun), "type": dtype, \
                "status": ", ".join(dstat_msg), "file": dfile, \
                "product": dprod, "block": dblock})
        occupied_ids.append(str(did))
        n += 1
    return device_list, occupied_ids


def sort_and_format_devices(device_list, occupied_ids):
    # Add padding devices and sort the list
    for id in range(8):
        if str(id) not in occupied_ids:
            device_list.append({"id": str(id), "un": "-", "type": "-", \
                    "status": "-", "file": "-", "product": "-", "block": "-"})

    # Sort list of devices by id
    device_list.sort(key=lambda dic: dic["id"][0])

    return device_list


def reserve_scsi_ids(reserved_scsi_ids):
    command = proto.PbCommand()
    command.operation = proto.PbOperation.RESERVE
    command.params.append(reserved_scsi_ids)

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
