import os
import logging

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
    from pathlib import PurePath
    prop_data = list_files(PROPERTIES_SUFFIX, cfg_dir)
    prop_files = [PurePath(x[0]).stem for x in prop_data]

    files = []
    for f in result.image_files_info.image_files:
        # Add properties meta data for the image, if applicable
        if f.name in prop_files:
            process = read_drive_properties(f"{cfg_dir}/{f.name}.{PROPERTIES_SUFFIX}")
            prop = process["conf"]
        else:
            prop = False
        size_mb = "{:,.1f}".format(f.size / 1024 / 1024)
        dtype = proto.PbDeviceType.Name(f.type)
        files.append(
                        {
                            "name": f.name,
                            "size": f.size,
                            "size_mb": size_mb,
                            "detected_type": dtype,
                            "prop": prop,
                        }
                    )

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
        return {"status": True, "msg": "File deleted"}
    else:
        return {"status": False, "msg": "Could not delete file"}


def unzip_file(file_name):
    """
    Takes str file_name
    Returns dict with boolean status and str msg
    """
    from subprocess import run
    server_info = get_server_info()

    unzip_proc = run(
        ["unzip", "-d", server_info["image_dir"], "-o", "-j", \
                f"{server_info['image_dir']}/{file_name}"], capture_output=True
    )
    if unzip_proc.returncode != 0:
        logging.warning(f"Unzipping failed: {unzip_proc}")
        return {"status": False, "msg": str(unzip_proc)}

    return {"status": True, "msg": f"Unzipped {file_name} to {server_info['image_dir']}"}


def download_file_to_iso(scsi_id, url):
    """
    Takes int scsi_id and str url
    Returns dict with boolean status and str msg
    """
    import urllib.request
    import urllib.error as error
    import time
    from subprocess import run

    server_info = get_server_info()

    file_name = url.split("/")[-1]
    tmp_ts = int(time.time())
    tmp_dir = "/tmp/" + str(tmp_ts) + "/"
    os.mkdir(tmp_dir)
    tmp_full_path = tmp_dir + file_name
    iso_filename = f"{server_info['image_dir']}/{file_name}.iso"

    try:
        urllib.request.urlretrieve(url, tmp_full_path)
    except (error.URLError, error.HTTPError, error.ContentTooShortError, FileNotFoundError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        return {"status": False, "msg": "Unknown error occurred."}

    # iso_filename = make_cd(tmp_full_path, None, None) # not working yet
    iso_proc = run(
        ["genisoimage", "-hfs", "-o", iso_filename, tmp_full_path], capture_output=True
    )
    if iso_proc.returncode != 0:
        return {"status": False, "msg": str(iso_proc)}

    process = attach_image(scsi_id, type="SCCD", image=iso_filename)
    return {"status": process["status"], "msg": process["msg"]}


def download_to_dir(url, save_dir):
    """
    Takes str url, str save_dir
    Returns dict with boolean status and str msg
    """
    import urllib.request
    import urllib.error as error

    file_name = url.split("/")[-1]
    full_path = f"{save_dir}/{file_name}"

    try:
        urllib.request.urlretrieve(url, full_path)
        return {"status": True, "msg": "Downloaded the URL"}
    except (error.URLError, error.HTTPError, error.ContentTooShortError, FileNotFoundError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        return {"status": False, "msg": "Unknown error occurred."}


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
        return {"status": True, "msg": f"Successfully wrote to file: {file_name}"}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        delete_file(file_name)
        return {"status": False, "msg": str(e)}
    except:
        logging.error(f"Could not write to file: {file_name}")
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
    Writes a drive property configuration file to the config dir.
    Takes file name base (str) and conf (list of dicts) as arguments
    Returns dict with boolean status and str msg
    """
    from json import dump
    file_path = cfg_dir + file_name
    try:
        with open(file_path, "w") as json_file:
            dump(conf, json_file, indent=4)
        return {"status": True, "msg": f"Successfully wrote to file: {file_path}"}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        delete_file(file_path)
        return {"status": False, "msg": str(e)}
    except:
        logging.error(f"Could not write to file: {file_path}")
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
            return {"status": True, "msg": f"Read data from file: {path_name}", "conf": conf}
    except (IOError, ValueError, EOFError, TypeError) as e:
        logging.error(str(e))
        return {"status": False, "msg": str(e)}
    except:
        logging.error(f"Could not read file: {file_name}")
        return {"status": False, "msg": f"Could not read file: {path_name}"}
