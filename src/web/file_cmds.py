import os
import logging

from ractl_cmds import (
    attach_image,
    detach_all,
    list_devices,
    send_pb_command,
)
from settings import *
import rascsi_interface_pb2 as proto


def list_files(file_types):
    files_list = []
    for path, dirs, files in os.walk(base_dir):
        # Only list selected file types
        files = [f for f in files if f.lower().endswith(file_types)]
        files_list.extend(
            [
                (
                    file,
                    os.path.getsize(os.path.join(path, file))
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


def list_images():
    command = proto.PbCommand()
    command.operation = proto.PbOperation.IMAGE_FILES_INFO

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    files = []
    for f in result.image_files_info.image_files:
        size_mb = "{:,.1f}".format(f.size / 1024 / 1024)
        files.append({"name": f.name, "size": f.size, "size_mb": size_mb})

    return {"status": result.status, "msg": result.msg, "files": files}


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
    from subprocess import run

    unzip_proc = run(
        ["unzip", "-d", base_dir, "-o", "-j", base_dir + file_name], capture_output=True
    )
    if unzip_proc.returncode != 0:
        logging.warning(f"Unzipping failed: {unzip_proc}")
        return {"status": False, "msg": unzip_proc}

    return {"status": True, "msg": f"{file_name} unzipped"}


def download_file_to_iso(scsi_id, url):
    import urllib.request
    import urllib.error as error
    import time
    from subprocess import run

    file_name = url.split("/")[-1]
    tmp_ts = int(time.time())
    tmp_dir = "/tmp/" + str(tmp_ts) + "/"
    os.mkdir(tmp_dir)
    tmp_full_path = tmp_dir + file_name
    iso_filename = base_dir + file_name + ".iso"

    try:
        urllib.request.urlretrieve(url, tmp_full_path)
    except (error.URLError, error.HTTPError, error.ContentTooShortError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        return {"status": False, "msg": "Error loading the URL"}

    # iso_filename = make_cd(tmp_full_path, None, None) # not working yet
    iso_proc = run(
        ["genisoimage", "-hfs", "-o", iso_filename, tmp_full_path], capture_output=True
    )
    if iso_proc.returncode != 0:
        return {"status": False, "msg": iso_proc}
    return attach_image(scsi_id, type="SCCD", image=iso_filename)


def download_image(url):
    import urllib.request
    import urllib.error as error

    file_name = url.split("/")[-1]
    full_path = base_dir + file_name

    try:
        urllib.request.urlretrieve(url, full_path)
        return {"status": True, "msg": "Downloaded the URL"}
    except (error.URLError, error.HTTPError, error.ContentTooShortError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        return {"status": False, "msg": "Error loading the URL"}


def pad_image(file_name, file_size, multiple):
    """
    Adds a number of bytes to the end of a file up to a size divisible by multiple
    Takes str file_name, int file_size, and int multiple
    """
    from subprocess import run

    file_path = base_dir + file_name
    if not file_size % multiple:
        return {"status": False, "msg": f"{file_name} does not need padding!"}
    target_size = file_size - (file_size % multiple) + multiple
    dd_proc = run(
            ["dd", "if=/dev/null", f"of={file_path}", "bs=1", "count=1", f"seek={target_size}" ], capture_output=True
    )
    if dd_proc.returncode != 0:
        return {"status": False, "msg": str(dd_proc)}
    return {"status": True, "msg": "Added " + str(target_size - file_size) + " bytes to " + file_name }


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
                device["params"] = dict(device["params"])
            dump(devices, json_file, indent=4)
        return {"status": True, "msg": f"Successfully wrote to file: {file_name}"}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
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
                kwargs = {"device_type": row["device_type"], \
                        "image": row["image"], "unit": int(row["un"]), \
                        "vendor": row["vendor"], "product": row["product"], \
                        "revision": row["revision"], "block_size": row["block_size"]}
                params = dict(row["params"])
                for p in params.keys():
                    kwargs[p] = params[p]
                process = attach_image(row["id"], **kwargs)
        if process["status"] == True:
            return {"status": process["status"], "msg": f"Successfully read from file: {file_name}"}
        else:
            return {"status": process["status"], "msg": process["msg"]}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        logging.error(f"Could not read file: {file_name}")
        return {"status": False, "msg": f"Could not read file: {file_name}"}


def write_drive_properties(file_name, conf):
    """
    Writes a drive property configuration file to the images dir.
    Takes file name base (str) and conf (list of dicts) as arguments
    """
    from json import dump
    try:
        with open(base_dir + file_name, "w") as json_file:
            dump(conf, json_file, indent=4)
        return {"status": True, "msg": f"Successfully wrote to file: {file_name}"}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        logging.error(f"Could not write to file: {file_name}")
        return {"status": False, "msg": f"Could not write to file: {file_name}"}



def read_drive_properties(path_name):
    """ 
    Reads drive properties to any dir.
    Either ones deployed to the images dir, or the canonical database. 
    Takes file path and bas (str) as argument
    """
    from json import load
    try:
        with open(path_name) as json_file:
            conf = load(json_file)
            return {"status": True, "msg": f"Read data from file: {path_name}", "conf": conf}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        logging.error(f"Could not read file: {file_name}")
        return {"status": False, "msg": f"Could not read file: {path_name}"}
