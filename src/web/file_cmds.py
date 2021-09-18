import os
import subprocess
import logging

from ractl_cmds import (
    attach_image,
    detach_all,
    list_devices,
    send_pb_command,
)
from settings import *
import rascsi_interface_pb2 as proto


def list_files():
    from fnmatch import translate
    valid_file_types = list(VALID_FILE_SUFFIX)
    valid_file_types = ["*." + s for s in valid_file_types]
    valid_file_types = r"|".join([translate(x) for x in valid_file_types])

    from re import match, IGNORECASE

    files_list = []
    for path, dirs, files in os.walk(base_dir):
        # Only list valid file types
        files = [f for f in files if match(valid_file_types, f, IGNORECASE)]
        files_list.extend(
            [
                (
                    os.path.join(path, file),
                    "{:,.1f}".format(
                        os.path.getsize(os.path.join(path, file)) / float(1 << 20),
                    ),
                    os.path.getsize(os.path.join(path, file)),
                )
                for file in files
            ]
        )
    return files_list


def list_config_files():
    files_list = []
    for root, dirs, files in os.walk(base_dir):
        for file in files:
            if file.endswith(".json"):
                files_list.append(file)
    return files_list


def create_new_image(file_name, file_type, size):
    command = proto.PbCommand()
    command.operation = proto.PbOperation.CREATE_IMAGE

    command.params["file"] = file_name + "." + file_type
    command.params["size"] = str(size)
    command.params["read_only"] = "false"

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def delete_file(file_name):
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DELETE_IMAGE

    command.params["file"] = file_name

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def unzip_file(file_name):
    import zipfile

    with zipfile.ZipFile(base_dir + file_name, "r") as zip_ref:
        zip_ref.extractall(base_dir)
        return True


def download_file_to_iso(scsi_id, url):
    import urllib.request
    import time

    file_name = url.split("/")[-1]
    tmp_ts = int(time.time())
    tmp_dir = "/tmp/" + str(tmp_ts) + "/"
    os.mkdir(tmp_dir)
    tmp_full_path = tmp_dir + file_name
    iso_filename = base_dir + file_name + ".iso"

    try:
        urllib.request.urlretrieve(url, tmp_full_path)
    except:
        # TODO: Capture a more descriptive error message
        return {"status": False, "msg": "Error loading the URL"}

    # iso_filename = make_cd(tmp_full_path, None, None) # not working yet
    iso_proc = subprocess.run(
        ["genisoimage", "-hfs", "-o", iso_filename, tmp_full_path], capture_output=True
    )
    if iso_proc.returncode != 0:
        return {"status": False, "msg": iso_proc}
    return attach_image(scsi_id, type="SCCD", image=iso_filename)


def download_image(url):
    import urllib.request

    file_name = url.split("/")[-1]
    full_path = base_dir + file_name

    try:
        urllib.request.urlretrieve(url, full_path)
        return {"status": True, "msg": "Downloaded the URL"}
    except:
        # TODO: Capture a more descriptive error message
        return {"status": False, "msg": "Error loading the URL"}


def write_config(file_name):
    from json import dump
    file_name = base_dir + file_name
    try:
        with open(file_name, "w") as json_file:
            devices = list_devices()["device_list"]
            for device in devices:
                # Remove keys that we don't want to store in the file
                del device["status"]
                del device["file"]
                # It's cleaner not to store an empty parameter for every device without media
                if device["image"] == "":
                    device["image"] = None
                # RaSCSI product names will be generated on the fly by RaSCSI
                if device["vendor"] == "RaSCSI":
                    device["vendor"] = device["product"] = device["revision"] = None
                # A block size of 0 is how RaSCSI indicates N/A for block size
                if device["block_size"] == 0:
                    device["block_size"] = None
                # Convert to a data type that can be serialized
                device["params"] = list(device["params"])
            dump(devices, json_file, indent=4)
        return {"status": True, "msg": f"Successfully wrote to file: {file_name}"}
    #TODO: more verbose error handling of file system errors
    except:
        raise
        logging.error(f"Could not write to file: {file_name}")
        return {"status": False, "msg": f"Could not write to file: {file_name}"}


def read_config(file_name):
    from json import load
    file_name = base_dir + file_name
    try:
        with open(file_name) as json_file:
            detach_all()
            devices = load(json_file)
            for row in devices:
                process = attach_image(row["id"], device_type=row["device_type"], image=row["image"], unit=int(row["un"]), \
                        params=row["params"], vendor=row["vendor"], product=row["product"], \
                        revision=row["revision"], block_size=row["block_size"])
        if process["status"] == True:
            return {"status": process["status"], "msg": f"Successfully read from file: {file_name}"}
        else:
            return {"status": process["status"], "msg": process["msg"]}
    #TODO: more verbose error handling of file system errors
    except:
        logging.error(f"Could not read file: {file_name}")
        return {"status": False, "msg": f"Could not read file: {file_name}"}


def write_sidecar(file_name, conf):
    ''' Writes a sidecar configuration file to the images dir. Takes file name (str) and conf (list of dicts) as arguments '''
    from json import dump
    file_name = base_dir + file_name + ".rascsi"
    try:
        with open(file_name, "w") as json_file:
            dump(conf, json_file, indent=4)
        return {"status": True, "msg": f"Successfully wrote to file: {file_name}"}
    #TODO: more verbose error handling of file system errors
    except:
        logging.error(f"Could not write to file: {file_name}")
        return {"status": False, "msg": f"Could not write to file: {file_name}"}



def read_sidecar(file_name):
    ''' Reads sidecar configurations, either ones deployed to the images dir, or the canonical database. Takes file name (str) as argument '''
    from json import load
    file_name = base_dir + file_name + ".rascsi"
    try:
        with open(file_name) as json_file:
            conf = load(json_file)
            return {"status": True, "msg": f"Read data from file: {file_name}", "conf": conf}
    #TODO: more verbose error handling of file system errors
    except:
        logging.error(f"Could not read file: {file_name}")
        return {"status": False, "msg": f"Could not read file: {file_name}"}
