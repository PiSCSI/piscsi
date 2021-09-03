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
    return list_devices()[int(scsi_id)]["type"]


def attach_image(scsi_id, image, image_type):
    if image_type == "SCCD" and get_type(scsi_id) == "SCCD":
        return insert(scsi_id, image)
    else:
        return subprocess.run(
            ["rasctl", "-c", "attach", "-t", image_type, "-i", scsi_id, "-f", image],
            capture_output=True,
        )


def detach_by_id(scsi_id):
    return subprocess.run(["rasctl", "-c" "detach", "-i", scsi_id], capture_output=True)


def detach_all():
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DETACH_ALL

    return_data = send_pb_command(command.SerializeToString())

    result_message = proto.PbResult().ParseFromString(return_data)

    logging.warning(result_message)
    #for scsi_id in range(0, 8):
    #    subprocess.run(["rasctl", "-c" "detach", "-i", str(scsi_id)])


def disconnect_by_id(scsi_id):
    return subprocess.run(
        ["rasctl", "-c", "disconnect", "-i", scsi_id], capture_output=True
    )


def eject_by_id(scsi_id):
    return subprocess.run(["rasctl", "-i", scsi_id, "-c", "eject"])


def insert(scsi_id, image):
    return subprocess.run(
        ["rasctl", "-i", scsi_id, "-c", "insert", "-f", image], capture_output=True
    )


def attach_daynaport(scsi_id):
    return subprocess.run(
        ["rasctl", "-i", scsi_id, "-c", "attach", "-t", "scdp"],
        capture_output=True,
    )


def is_bridge_setup(interface):
    process = subprocess.run(["brctl", "show"], capture_output=True)
    output = process.stdout.decode("utf-8")
    if "rascsi_bridge" in output and interface in output:
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
    scsi_ids = ",".join(list(reserved_scsi_ids))
    return subprocess.run(["rasctl", "-r", scsi_ids])

def send_pb_command(payload):
    import socket
    import struct

    HOST = 'localhost'
    PORT = 6868

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((HOST, PORT))
        s.send(struct.pack("<i", len(payload)))
        s.send(payload)
        response_header = struct.unpack("<i", s.recv(4))[0]
        response_data = s.recv(response_header)

    return response_data
