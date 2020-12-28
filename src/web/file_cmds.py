import fnmatch
import os
import subprocess
import time

from create_disk import make_cd
from ractl_cmds import attach_image

base_dir = "/home/pi/images/"  # Default
valid_file_types = ['*.hda', '*.iso', '*.cdr']
valid_file_types = r'|'.join([fnmatch.translate(x) for x in valid_file_types])


def create_new_image(file_name, size):
    if file_name == "":
        file_name = "new_file." + str(int(time.time())) + ".hda"
    if not file_name.endswith(".hda"):
        file_name = file_name + ".hda"

    return subprocess.run(["dd", "if=/dev/zero", "of=" + base_dir + file_name, "bs=1M", "count=" + size],
                          capture_output=True)


def delete_image(file_name):
    full_path = base_dir + file_name
    if os.path.exists(full_path):
        os.remove(full_path)
        return True
    else:
        return False


def unzip_file(file_name):
    import zipfile
    with zipfile.ZipFile(base_dir + file_name, 'r') as zip_ref:
        zip_ref.extractall(base_dir)
        return True

def rascsi_service(action):
    # start/stop/restart
    return subprocess.run(["sudo", "/bin/systemctl", action, "rascsi.service"]).returncode == 0


def download_file_to_iso(scsi_id, url):
    import urllib.request
    file_name = url.split('/')[-1]
    tmp_ts = int(time.time())
    tmp_dir = "/tmp/" + str(tmp_ts) + "/"
    os.mkdir(tmp_dir)
    tmp_full_path = tmp_dir + file_name
    iso_filename = base_dir + file_name + ".iso"

    urllib.request.urlretrieve(url, tmp_full_path)
    # iso_filename = make_cd(tmp_full_path, None, None) # not working yet
    iso_proc = subprocess.run(["genisoimage", "-hfs", "-o", iso_filename, tmp_full_path], capture_output=True)
    if iso_proc.returncode != 0:
        return iso_proc
    return attach_image(scsi_id, iso_filename, "cd")


def download_image(url):
    import urllib.request
    file_name = url.split('/')[-1]
    full_path = base_dir + file_name

    urllib.request.urlretrieve(url, full_path)