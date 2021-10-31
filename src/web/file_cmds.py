import os
import logging
from pathlib import PurePath

from ractl_cmds import (
    get_server_info,
    attach_image,
    detach_all,
    list_devices,
    send_pb_command,
)
from settings import *
import rascsi_interface_pb2 as proto


def list_files(file_types, dir_path):
    """
    Takes a list or tuple of str file_types - e.g. ('hda', 'hds')
    Returns list of lists files_list:
    index 0 is str file name and index 1 is int size in bytes
    """
    files_list = []
    for path, dirs, files in os.walk(dir_path):
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
    """
    Returns a list of RaSCSI config files in cfg_dir:
    list of str files_list
    """
    files_list = []
    for root, dirs, files in os.walk(cfg_dir):
        for file in files:
            if file.endswith(".json"):
                files_list.append(file)
    return files_list


def list_images():
    """
    Sends a IMAGE_FILES_INFO command to the server
    Returns a dict with boolean status, str msg, and list of dicts files

    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEFAULT_IMAGE_FILES_INFO

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)

    # Get a list of all *.properties files in cfg_dir
    prop_data = list_files(PROPERTIES_SUFFIX, cfg_dir)
    prop_files = [PurePath(x[0]).stem for x in prop_data]

    from zipfile import ZipFile, is_zipfile
    server_info = get_server_info()
    files = []
    for f in result.image_files_info.image_files:
        # Add properties meta data for the image, if applicable
        if f.name in prop_files:
            process = read_drive_properties(f"{cfg_dir}/{f.name}.{PROPERTIES_SUFFIX}")
            prop = process["conf"]
        else:
            prop = False
        if f.name.lower().endswith(".zip"):
            zip_path = f"{server_info['image_dir']}/{f.name}"
            if is_zipfile(zip_path):
                zipfile = ZipFile(zip_path)
                # Get a list of str containing all zipfile members
                zip_members = zipfile.namelist()
                # Strip out directories from the list
                zip_members = [x for x in zip_members if not x.endswith("/")]
            else:
                logging.warning("%s is an invalid zip file", zip_path)
                zip_members = False
        else:
            zip_members = False

        size_mb = "{:,.1f}".format(f.size / 1024 / 1024)
        dtype = proto.PbDeviceType.Name(f.type)
        files.append({
            "name": f.name,
            "size": f.size,
            "size_mb": size_mb,
            "detected_type": dtype,
            "prop": prop,
            "zip": zip_members,
            })

    return {"status": result.status, "msg": result.msg, "files": files}


def create_new_image(file_name, file_type, size):
    """
    Takes str file_name, str file_type, and int size
    Sends a CREATE_IMAGE command to the server
    Returns dict with boolean status and str msg
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.CREATE_IMAGE

    command.params["file"] = file_name + "." + file_type
    command.params["size"] = str(size)
    command.params["read_only"] = "false"

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def delete_image(file_name):
    """
    Takes str file_name
    Sends a DELETE_IMAGE command to the server
    Returns dict with boolean status and str msg
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DELETE_IMAGE

    command.params["file"] = file_name

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def delete_file(file_path):
    """
    Takes str file_path with the full path to the file to delete
    Returns dict with boolean status and str msg
    """
    if os.path.exists(file_path):
        os.remove(file_path)
        return {"status": True, "msg": f"File deleted: {file_path}"}
    return {"status": False, "msg": f"File to delete not found: {file_path}"}


def unzip_file(file_name, member=False):
    """
    Takes (str) file_name, optional (str) member
    Returns dict with (boolean) status and (list of str) msg
    """
    from subprocess import run
    from re import escape
    server_info = get_server_info()

    if not member:
        unzip_proc = run(
            ["unzip", "-d", server_info["image_dir"], "-n", "-j", \
                f"{server_info['image_dir']}/{file_name}"], capture_output=True
            )
    else:
        unzip_proc = run(
            ["unzip", "-d", server_info["image_dir"], "-n", "-j", \
                f"{server_info['image_dir']}/{file_name}", escape(member)], capture_output=True
            )
    if unzip_proc.returncode != 0:
        stderr = unzip_proc.stderr.decode("utf-8")
        logging.warning("Unzipping failed: %s", stderr)
        return {"status": False, "msg": stderr}

    from re import findall
    unzipped = findall(
        "(?:inflating|extracting):(.+)\n",
        unzip_proc.stdout.decode("utf-8")
        )
    return {"status": True, "msg": unzipped}


def download_file_to_iso(url):
    """
    Takes int scsi_id and str url
    Returns dict with boolean status and str msg
    """
    from time import time
    from subprocess import run

    server_info = get_server_info()

    file_name = PurePath(url).name
    tmp_ts = int(time())
    tmp_dir = "/tmp/" + str(tmp_ts) + "/"
    os.mkdir(tmp_dir)
    tmp_full_path = tmp_dir + file_name
    iso_filename = f"{server_info['image_dir']}/{file_name}.iso"

    req_proc = download_to_dir(url, tmp_dir)

    if not req_proc["status"]:
        return {"status": False, "msg": req_proc["msg"]}

    iso_proc = run(
        ["genisoimage", "-hfs", "-o", iso_filename, tmp_full_path], capture_output=True
    )
    if iso_proc.returncode != 0:
        return {"status": False, "msg": iso_proc.stderr.decode("utf-8")}

    return {"status": True, "msg": iso_proc.stdout.decode("utf-8"), "file_name": iso_filename}


def download_to_dir(url, save_dir):
    """
    Takes str url, str save_dir
    Returns dict with boolean status and str msg
    """
    import requests
    file_name = PurePath(url).name
    logging.info("Making a request to download %s", url)

    try:
        with requests.get(url, stream=True, headers={"User-Agent": "Mozilla/5.0"}) as req:
            req.raise_for_status()
            with open(f"{save_dir}/{file_name}", "wb") as download:
                for chunk in req.iter_content(chunk_size=8192):
                    download.write(chunk)
    except requests.exceptions.RequestException as e:
        logging.warning("Request failed: %s", str(e))
        return {"status": False, "msg": str(e)}

    logging.info("Response encoding: %s", req.encoding)
    logging.info("Response content-type: %s", req.headers["content-type"])
    logging.info("Response status code: %s", req.status_code)

    return {"status": True, "msg": f"File downloaded from {url} to {save_dir}"}


def write_config(file_name):
    """
    Takes str file_name
    Returns dict with boolean status and str msg
    """
    from json import dump
    file_name = cfg_dir + file_name
    try:
        with open(file_name, "w") as json_file:
            devices = list_devices()["device_list"]
            if len(devices) == 0:
                return {"status": False, "msg": "No attached devices."}
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
        return {"status": True, "msg": f"Saved config to {file_name}"}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        delete_file(file_name)
        return {"status": False, "msg": str(e)}
    except:
        logging.error("Could not write to file: %s", file_name)
        delete_file(file_name)
        return {"status": False, "msg": f"Could not write to file: {file_name}"}


def read_config(file_name):
    """
    Takes str file_name
    Returns dict with boolean status and str msg
    """
    from json import load
    file_name = cfg_dir + file_name
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
        if process["status"]:
            return {"status": process["status"], "msg": f"Loaded config from: {file_name}"}
        return {"status": process["status"], "msg": process["msg"]}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        logging.error("Could not read file: %s", file_name)
        return {"status": False, "msg": f"Could not read file: {file_name}"}


def write_drive_properties(file_name, conf):
    """
    Writes a drive property configuration file to the config dir.
    Takes file name base (str) and conf (list of dicts) as arguments
    Returns dict with boolean status and str msg
    """
    from json import dump
    file_path = cfg_dir + file_name
    try:
        with open(file_path, "w") as json_file:
            dump(conf, json_file, indent=4)
        return {"status": True, "msg": f"Created file: {file_path}"}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        delete_file(file_path)
        return {"status": False, "msg": str(e)}
    except:
        logging.error("Could not write to file: %s", file_path)
        delete_file(file_path)
        return {"status": False, "msg": f"Could not write to file: {file_path}"}



def read_drive_properties(path_name):
    """
    Reads drive properties from json formatted file.
    Takes (str) path_name as argument.
    Returns dict with boolean status, str msg, dict conf
    """
    from json import load
    try:
        with open(path_name) as json_file:
            conf = load(json_file)
            return {"status": True, "msg": f"Read from file: {path_name}", "conf": conf}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        logging.error("Could not read file: %s", path_name)
        return {"status": False, "msg": f"Could not read file: {path_name}"}
