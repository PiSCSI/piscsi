import fnmatch
import os
import subprocess
import re

base_dir = "/home/pi/images"  # Default
valid_file_types = ['*.hda', '*.iso', '*.cdr', '*.zip']
valid_file_types = r'|'.join([fnmatch.translate(x) for x in valid_file_types])


def is_active():
    process = subprocess.run(["systemctl", "is-active", "rascsi"], capture_output=True)
    return process.stdout.decode("utf-8").strip() == "active"


def list_files():
    files_list = []
    for path, dirs, files in os.walk(base_dir):
        # Only list valid file types
        files = [f for f in files if re.match(valid_file_types, f)]
        files_list.extend([
            (os.path.join(path, file),
             # TODO: move formatting to template
             '{:,.0f}'.format(os.path.getsize(os.path.join(path, file)) / float(1 << 20)) + " MB")
            for file in files])
    return files_list


def attach_image(scsi_id, image, type):
    return subprocess.run(["rasctl", "-c", "attach", "-t", type, "-i", scsi_id, "-f", image], capture_output=True)


def detach_by_id(scsi_id):
    return subprocess.run(["rasctl", "-c" "detach", "-i", scsi_id], capture_output=True)


def disconnect_by_id(scsi_id):
    return subprocess.run(["rasctl", "-c", "disconnect", "-i", scsi_id], capture_output=True)


def eject_by_id(scsi_id):
    return subprocess.run(["rasctl", "-i", scsi_id, "-c", "eject"])


def insert(scsi_id, file_name):
    return subprocess.run(["rasctl", "-i", scsi_id, "-c", "insert", "-f", base_dir + file_name], capture_output=True)


def rascsi_service(action):
    # start/stop/restart
    return subprocess.run(["sudo", "/bin/systemctl", action, "rascsi.service"]).returncode == 0


def list_devices():
    device_list = []
    for id in range(8):
        device_list.append({"id": str(id), "un": "-", "type": "-", "file": "-"})
    output = subprocess.run(["rasctl", "-l"], capture_output=True).stdout.decode("utf-8")
    for line in output.splitlines():
        # Valid line to process, continue
        if not line.startswith("+") and \
                not line.startswith("| ID |") and \
                (not line.startswith("No device is installed.") or line.startswith("No images currently attached.")) \
                and len(line) > 0:
            line.rstrip()
            device = {}
            segments = line.split("|")
            if len(segments) > 4:
                idx = int(segments[1].strip())
                device_list[idx]["id"] = str(idx)
                device_list[idx]['un'] = segments[2].strip()
                device_list[idx]['type'] = segments[3].strip()
                device_list[idx]['file'] = segments[4].strip()

    return device_list