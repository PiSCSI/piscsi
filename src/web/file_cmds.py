import fnmatch
import os
import subprocess
import time
import io
import re
import sys

from ractl_cmds import (
    attach_image,
    detach_all,
    list_devices,
)
from settings import *


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


def rascsi_service(action):
    # start/stop/restart
    return (
        subprocess.run(["sudo", "/bin/systemctl", action, "rascsi.service"]).returncode
        == 0
    )


def download_file_to_iso(scsi_id, url):
    import urllib.request

    file_name = url.split("/")[-1]
    tmp_ts = int(time.time())
    tmp_dir = "/tmp/" + str(tmp_ts) + "/"
    os.mkdir(tmp_dir)
    tmp_full_path = tmp_dir + file_name
    iso_filename = base_dir + file_name + ".iso"

    urllib.request.urlretrieve(url, tmp_full_path)
    # iso_filename = make_cd(tmp_full_path, None, None) # not working yet
    iso_proc = subprocess.run(
        ["genisoimage", "-hfs", "-o", iso_filename, tmp_full_path], capture_output=True
    )
    if iso_proc.returncode != 0:
        return iso_proc
    return attach_image(scsi_id, iso_filename, "SCCD")


def download_image(url):
    import urllib.request

    file_name = url.split("/")[-1]
    full_path = base_dir + file_name

    urllib.request.urlretrieve(url, full_path)

def write_config_csv(file_name):
    import csv

    # This method takes the output of 'rasctl -l' and parses it into csv format:
    # 0: ID
    # 1: Unit Number (unused in rascsi-web)
    # 2: Device Type
    # 3: Device Status (includes the path to a loaded image file)
    # TODO: Remove the dependence on rasctl; e.g. when implementing protobuf for rascsi-web
    try:
        with open(file_name, "w") as csv_file:
            writer = csv.writer(csv_file)
            for device in list_devices():
                if device["type"] != "-":
                    device_info = list (device.values())
                    # Match a *nix file path inside column 3, cutting out the last chunk that starts with a space
                    filesearch = re.search("(^(/[^/ ]*)+)(\s.*)*$", device_info[3])
                    if filesearch is None:
                        device_info[3] = ""
                    else:
                        device_info[3] = filesearch.group(1)
                    writer.writerow(device_info)
    except:
        print ("Could not open file for writing: ", file_name)
				
def read_config_csv(file_name):
    detach_all()
    import csv

    try:
        with open(file_name) as csv_file:
            config_reader = csv.reader(csv_file)
            #TODO: Remove hard-coded string sanitation (e.g. after implementing protobuf)
            exclude_list = ("X68000 HOST BRIDGE", "DaynaPort SCSI/Link", " (WRITEPROTECT)", "NO MEDIA")
            for row in config_reader:
                image_name = row[3]
                for e in exclude_list:
                    image_name = image_name.replace(e, "")
                attach_image(row[0], image_name, row[2])
    except:
        print ("Could not access file: ", file_name)
