import fnmatch
import subprocess
import re
import logging

from settings import *
import rascsi_interface_pb2 as proto

valid_file_suffix = ["*.hda", "*.hdn", "*.hdi", "*.nhd", "*.hdf", "*.hds", "*.hdr", "*.iso", "*.cdr", "*.toast", "*.img", "*.zip"]
valid_file_types = r"|".join([fnmatch.translate(x) for x in valid_file_suffix])


def is_active():
    process = subprocess.run(["systemctl", "is-active", "rascsi"], capture_output=True)
    return process.stdout.decode("utf-8").strip() == "active"


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


def get_valid_scsi_ids(devices, invalid_list):
    for device in devices:
        if device["file"] != "NO MEDIA" and device["file"] != "-":
            invalid_list.append(int(device["id"]))

    valid_list = list(range(8))
    for id in invalid_list:
        try:
            valid_list.remove(int(id))
        except:
            logging.warning("Invalid SCSI id " + str(id))
    valid_list.reverse()
    return valid_list


def get_type(scsi_id):
    command = proto.PbDevice()
    command.id = int(scsi_id)

    return_data = send_pb_command(command.SerializeToString())
    logging.warning(return_data)
    result_message = proto.PbDeviceDefinition().ParseFromString(return_data)

    logging.warning(result_message)
    return result_message


def attach_image(scsi_id, image, image_type):
    if image_type == "SCCD" and get_type(scsi_id) == "SCCD":
        return insert(scsi_id, image)
    else:
        command = proto.PbCommand()
        devices = proto.PbDeviceDefinition()
        devices.id = int(scsi_id)
        devices.params.append(image)
        command.operation = proto.PbOperation.ATTACH
        command.devices.append(devices)

        return_data = send_pb_command(command.SerializeToString())
        result_message = proto.PbResult().ParseFromString(return_data)
        return result_message


def detach_by_id(scsi_id):
    command = proto.PbCommand()
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    command.devices.append(devices)

    command.operation = proto.PbOperation.DETACH

    return_data = send_pb_command(command.SerializeToString())

    result_message = proto.PbResult().ParseFromString(return_data)
    return result_message


def detach_all():
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DETACH_ALL

    return_data = send_pb_command(command.SerializeToString())

    result_message = proto.PbResult().ParseFromString(return_data)
    return result_message


def eject_by_id(scsi_id):
    command = proto.PbCommand()
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)

    command.operation = proto.PbOperation.EJECT
    command.devices.append(devices)

    return_data = send_pb_command(command.SerializeToString())

    result_message = proto.PbResult().ParseFromString(return_data)
    return result_message


def insert(scsi_id, image):
    command = proto.PbCommand()
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    devices.params.append(image)
    command.devices.append(devices)
    command.operation = proto.PbOperation.INSERT

    return_data = send_pb_command(command.SerializeToString())
    result_message = proto.PbResult().ParseFromString(return_data)
    return result_message


def attach_daynaport(scsi_id):
    command = proto.PbCommand()
    devices = proto.PbDeviceDefinition()
    devices.id = int(scsi_id)
    devices.type = proto.PbDeviceType.SCDP
    command.devices.append(devices)
    command.operation = proto.PbOperation.ATTACH

    return_data = send_pb_command(command.SerializeToString())
    if return_data != False:
        result_message = proto.PbResult().ParseFromString(return_data)
        return result_message
    else:
        return 0


def is_bridge_setup(interface):
    process = subprocess.run(["brctl", "show"], capture_output=True)
    output = process.stdout.decode("utf-8")
    if "rascsi_bridge" in output:
        return True
    return False


def daynaport_setup_bridge(interface):
    return subprocess.run(
        [f"{base_dir}../RASCSI/src/raspberrypi/setup_bridge.sh", interface],
        capture_output=True,
    )


def rascsi_service(action):
    # start/stop/restart
    return (
        subprocess.run(["sudo", "/bin/systemctl", action, "rascsi.service"]).returncode
        == 0
    )


def list_devices():
    device_list = []
    for id in range(8):
        device_list.append({"id": str(id), "un": "-", "type": "-", "file": "-"})
    output = subprocess.run(["rasctl", "-l"], capture_output=True).stdout.decode(
        "utf-8"
    )
    for line in output.splitlines():
        # Valid line to process, continue
        if (
            not line.startswith("+")
            and not line.startswith("| ID |")
            and (
                not line.startswith("No device is installed.")
                or line.startswith("No images currently attached.")
            )
            and len(line) > 0
        ):
            line.rstrip()
            device = {}
            segments = line.split("|")
            if len(segments) > 4:
                idx = int(segments[1].strip())
                device_list[idx]["id"] = str(idx)
                device_list[idx]["un"] = segments[2].strip()
                device_list[idx]["type"] = segments[3].strip()
                device_list[idx]["file"] = segments[4].strip()

    return device_list


def reserve_scsi_ids(reserved_scsi_ids):
    command = proto.PbCommand()
    command.operation = proto.PbOperation.RESERVE
    command.params.append(reserved_scsi_ids)

    return_data = send_pb_command(command.SerializeToString())
    if return_data != False:
        result_message = proto.PbResult().ParseFromString(return_data)
        return result_message
    else:
        return 0


def send_pb_command(payload):
    import socket
    import struct

    # Host and port number where rascsi is listening for socket connections
    HOST = 'localhost'
    PORT = 6868

    counter = 0
    while counter < 100:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((HOST, PORT))
                # Prepending a little endian 32bit header with the message size
                s.send(struct.pack("<i", len(payload)))
                s.send(payload)
                response = s.recv(4)
                if len(response) >= 4:
                    # Extracting the response header to get the length of the response message
                    response_length = struct.unpack("<i", response)[0]
                    logging.warning("Response length " + str(response_length) + str(type(response_length)))
                    chunks = []
                    bytes_recvd = 0
                    while bytes_recvd < response_length:
                        chunk = s.recv(min(response_length - bytes_recvd, 2048))
                        if chunk == b'':
                            logging.error("Socket connection dropped.")
                            return 0
                        chunks.append(chunk)
                        bytes_recvd = bytes_recvd + len(chunk)
                        logging.warning("read bytes" + str(bytes_recvd))
                    response_message = b''.join(chunks)
                    logging.warning(response_message)
                    return response_message
                else:
                    logging.error("Missing protobuf data.")
                    return 0
        except socket.error as error:
            logging.error("Failed to connect to the RaSCSI service: {error}")
            counter += 1
