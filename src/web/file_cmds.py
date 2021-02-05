import fnmatch
import os
import subprocess
import time

from ractl_cmds import attach_image
from settings import *

valid_file_types = ["*.hda", "*.iso", "*.cdr"]
valid_file_types = r"|".join([fnmatch.translate(x) for x in valid_file_types])


def create_new_image(file_name, type, size, sub_dir):
    if file_name == "":
        file_name = "new_image." + str(int(time.time())) + "." + type
    else:
        file_name = file_name + "." + type

    return subprocess.run(
        ["dd", "if=/dev/zero", f"of={base_dir}{sub_dir}/{file_name}", "bs=1M", "count=" + size],
        capture_output=True,
    )


def delete_image(file_name):
    full_path = base_dir + file_name
    if os.path.exists(full_path):
        os.remove(full_path)
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

    iso_proc = make_iso_from_file(tmp_full_path)
    if iso_proc.returncode != 0:
        return iso_proc
    return attach_image(scsi_id, iso_filename, "SCCD")


def make_iso_from_file(file_path):
    iso_filename = f"{base_dir}{get_name_from_path(file_path)}.iso"
    return subprocess.run(
        ["genisoimage", "-hfs", "-o", iso_filename, file_path], capture_output=True
    )


def get_name_from_path(file_path):
    return file_path.split("/")[-1]


def download_image(url):
    import urllib.request

    file_name = get_name_from_path(url)
    full_path = base_dir + file_name

    urllib.request.urlretrieve(url, full_path)
