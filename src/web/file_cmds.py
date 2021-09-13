import os
import subprocess
import time
import logging

from ractl_cmds import (
    attach_image,
    detach_all,
    list_devices,
)
from settings import *


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
            if file.endswith(".json"):
                files_list.append(file)
    return files_list


def create_new_image(file_name, type, size):
    if file_name == "":
        file_name = "new_image." + str(int(time.time())) + "." + type
    else:
        file_name = file_name + "." + type

    return subprocess.run(
        ["truncate", "--size", f"{size}m", f"{base_dir}{file_name}"],
        capture_output=True,
    )


def delete_file(file_name):
    if os.path.exists(file_name):
        os.remove(file_name)
        return True
    else:
        return False


def unzip_file(file_name):
    import zipfile

    with zipfile.ZipFile(base_dir + file_name, "r") as zip_ref:
        zip_ref.extractall(base_dir)
        return True


def download_file_to_iso(scsi_id, url):
    import urllib.request

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
    return attach_image(scsi_id, "SCCD", iso_filename)


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
    try:
        with open(file_name, "w") as json_file:
            devices = list_devices()[0]
            for device in devices:
                # Remove keys that we don't want to store in the file
                del device["status"]
                del device["file"]
                # It's cleaner not to store an empty parameter for every device without media
                if device["path"] == "":
                    device["path"] = None
                # RaSCSI product names will be generated on the fly by RaSCSI
                if device["vendor"] == "RaSCSI":
                    device["vendor"] = device["product"] = device["revision"] = None
                # A block size of 0 is how RaSCSI indicates N/A for block size
                if device["block"] == 0:
                    device["block"] = None
                # Convert to a data type that can be serialized
                device["params"] = list(device["params"])
            dump(devices, json_file, indent=4)
        return {"status": True, "msg": f"Successfully wrote to file: {file_name}"}
    #TODO: more verbose error handling
    except:
        logging.error(f"Could not write to file: {file_name}")
        return {"status": False, "msg": f"Could not write to file: {file_name}"}


def read_config(file_name):
    from json import load
    try:
        with open(file_name) as json_file:
            detach_all()
            devices = load(json_file)
            for row in devices:
                attach_image(row["id"], row["type"], row["path"], int(row["un"]), \
                        row["params"], row["vendor"], row["product"], \
                        row["revision"], row["block"])
        return {"status": True, "msg": f"Successfully read from file: {file_name}"}
    #TODO: more verbose error handling
    except:
        logging.error(f"Could not read file: {file_name}")
        return {"status": False, "msg": f"Could not read file: {file_name}"}
