import fnmatch
import subprocess
import re
import logging
import socket
import struct

from settings import *
import rascsi_interface_pb2 as proto

valid_file_suffix = ["*.hda", "*.hdn", "*.hdi", "*.nhd", "*.hdf", "*.hds", "*.hdr", "*.iso", "*.cdr", "*.toast", "*.img", "*.zip"]
valid_file_types = r"|".join([fnmatch.translate(x) for x in valid_file_suffix])


def is_active():
    process = subprocess.run(["systemctl", "is-active", "rascsi"], capture_output=True)
    return process.stdout.decode("utf-8").strip() == "active"


def rascsi_version():
    command = proto.PbCommand()
    command.operation = proto.PbOperation.SERVER_INFO

    data = send_pb_command(command.SerializeToString())

    result = proto.PbResult()
    result.ParseFromString(data)
    majv = result.server_info.major_version
    minv = result.server_info.minor_version
    patv = result.server_info.patch_version
    return {"status": result.status, "msg": str(majv) + "." + str(minv) + "." + str(patv)}
    

def list_files():
    files_list = []
    for path, dirs, files in os.walk(base_dir):
        # Only list valid file types
        files = [f for f in files if re.match(valid_file_types, f)]
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


def get_valid_scsi_ids(devices, invalid_list, occupied_ids):
    logging.warning(str(invalid_list))
    logging.warning(str(occupied_ids))
    for device in devices:
        if "Removed" in device["status"] and device["type"] in ["SCCD", "SCMO"]:
            occupied_ids.remove(device["id"])

    invalid_ids = invalid_list + occupied_ids
    valid_ids = list(range(8))
    for id in invalid_ids:
        valid_ids.remove(int(id))
    valid_ids.reverse()
    return valid_ids


def get_type(scsi_id):
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICE_INFO
    device = proto.PbDeviceDefinition()
    device.id = int(scsi_id)
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
        command = proto.PbCommand()
        devices = proto.PbDeviceDefinition()
        devices.id = int(scsi_id)
        devices.type = proto.PbDeviceType.Value(image_type)
        devices.params.append(image)
        command.operation = proto.PbOperation.ATTACH
        command.devices.append(devices)

        data = send_pb_command(command.SerializeToString())

        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}


def detach_by_id(scsi_id):
    command = proto.PbCommand()
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    command.devices.append(devices)

    command.operation = proto.PbOperation.DETACH

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
    command = proto.PbCommand()
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)

    command.operation = proto.PbOperation.EJECT
    command.devices.append(devices)

    data = send_pb_command(command.SerializeToString())

    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def insert(scsi_id, image):
    command = proto.PbCommand()
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    devices.params.append(image)
    command.devices.append(devices)
    command.operation = proto.PbOperation.INSERT

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def attach_daynaport(scsi_id):
    command = proto.PbCommand()
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    devices.type = proto.PbDeviceType.SCDP
    command.devices.append(devices)
    command.operation = proto.PbOperation.ATTACH

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def is_bridge_setup():
    process = subprocess.run(["brctl", "show"], capture_output=True)
    output = process.stdout.decode("utf-8")
    if "rascsi_bridge" in output:
        return True
    return False


def rascsi_service(action):
    # start/stop/restart
    return (
        subprocess.run(["sudo", "/bin/systemctl", action, "rascsi.service"]).returncode
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
        dstat_msg = []
        if dstat.protected == True:
            dstat_msg.append("Protected")
        if dstat.removed == True:
            dstat_msg.append("Removed")
        if dstat.locked == True:
            dstat_msg.append("Locked")
        dfile = result.device_info.devices[n].file.name
        dprod = result.device_info.devices[n].vendor + " " + result.device_info.devices[n].product + " " + result.device_info.devices[n].revision
        dblock = result.device_info.devices[n].block_size
        # TODO: Move formatting elsewhere
        device_list.append({"id": str(did), "un": str(dun), "type": dtype, "status": ", ".join(dstat_msg), "file": dfile, "product": dprod, "block": dblock})
        occupied_ids.append(did)
        n += 1
    # TODO: Want to go back to returning only one variable here
    return [device_list, occupied_ids]


def sort_and_format_devices(device_list, occupied_ids):
    # Add padding devices and sort the list
    for id in range(8):
        if id not in occupied_ids:
            device_list.append({"id": str(id), "un": "-", "type": "-", "status": "-", "file": "-", "product": "-", "block": "-"})

    # Sort list of devices by id
    device_list.sort(key=lambda tup: tup["id"][0])

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
    while counter < 100:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((HOST, PORT))
                send_over_socket(s, payload)
                return recv_from_socket(s)
        except socket.error as error:
            logging.error("Failed to connect to the RaSCSI service: " + str(error))
            counter += 1
    # TODO: Fail the webapp gracefully when connection fails
    return {"status": False, "msg": "The RaSCSI service does not seem to be running."}


def send_over_socket(s, payload):
    # Prepending a little endian 32bit header with the message size
    s.send(struct.pack("<i", len(payload)))
    s.send(payload)


def recv_from_socket(s):
    # Receive the first 4 bytes to get the response header
    response = s.recv(4)
    if len(response) >= 4:
        # Extracting the response header to get the length of the response message
        response_length = struct.unpack("<i", response)[0]
        # Reading in chunks, to handle a case where the response message is very large
        chunks = []
        bytes_recvd = 0
        while bytes_recvd < response_length:
            chunk = s.recv(min(response_length - bytes_recvd, 2048))
            if chunk == b'':
                logging.error("Socket connection dropped.")
                return False
            chunks.append(chunk)
            bytes_recvd = bytes_recvd + len(chunk)
        response_message = b''.join(chunks)
        return response_message
    else:
        logging.error("Missing protobuf data.")
        return False

