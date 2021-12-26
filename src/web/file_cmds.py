"""
Module for methods reading from and writing to the file system
"""

import os
import logging
from pathlib import PurePath
from flask import current_app
from flask_babel import _

from ractl_cmds import (
    get_server_info,
    get_reserved_ids,
    attach_image,
    detach_all,
    list_devices,
    reserve_scsi_ids,
)
from pi_cmds import run_async
from socket_cmds import send_pb_command
from settings import CFG_DIR, CONFIG_FILE_SUFFIX, PROPERTIES_SUFFIX, RESERVATIONS
import rascsi_interface_pb2 as proto


def list_files(file_types, dir_path):
    """
    Takes a (list) or (tuple) of (str) file_types - e.g. ('hda', 'hds')
    Returns (list) of (list)s files_list:
    index 0 is (str) file name and index 1 is (int) size in bytes
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
    Finds fils with file ending CONFIG_FILE_SUFFIX in CFG_DIR.
    Returns a (list) of (str) files_list
    """
    files_list = []
    for root, dirs, files in os.walk(CFG_DIR):
        for file in files:
            if file.endswith("." + CONFIG_FILE_SUFFIX):
                files_list.append(file)
    return files_list


def list_images():
    """
    Sends a IMAGE_FILES_INFO command to the server
    Returns a (dict) with (bool) status, (str) msg, and (list) of (dict)s files

    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEFAULT_IMAGE_FILES_INFO
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)

    # Get a list of all *.properties files in CFG_DIR
    prop_data = list_files(PROPERTIES_SUFFIX, CFG_DIR)
    prop_files = [PurePath(x[0]).stem for x in prop_data]

    from zipfile import ZipFile, is_zipfile
    server_info = get_server_info()
    files = []
    for file in result.image_files_info.image_files:
        # Add properties meta data for the image, if applicable
        if file.name in prop_files:
            process = read_drive_properties(f"{CFG_DIR}/{file.name}.{PROPERTIES_SUFFIX}")
            prop = process["conf"]
        else:
            prop = False
        if file.name.lower().endswith(".zip"):
            zip_path = f"{server_info['image_dir']}/{file.name}"
            if is_zipfile(zip_path):
                zipfile = ZipFile(zip_path)
                # Get a list of (str) containing all zipfile members
                zip_members = zipfile.namelist()
                # Strip out directories from the list
                zip_members = [x for x in zip_members if not x.endswith("/")]
            else:
                logging.warning("%s is an invalid zip file", zip_path)
                zip_members = False
        else:
            zip_members = False

        size_mb = "{:,.1f}".format(file.size / 1024 / 1024)
        dtype = proto.PbDeviceType.Name(file.type)
        files.append({
            "name": file.name,
            "size": file.size,
            "size_mb": size_mb,
            "detected_type": dtype,
            "prop": prop,
            "zip_members": zip_members,
            })

    return {"status": result.status, "msg": result.msg, "files": files}


def create_new_image(file_name, file_type, size):
    """
    Takes (str) file_name, (str) file_type, and (int) size
    Sends a CREATE_IMAGE command to the server
    Returns (dict) with (bool) status and (str) msg
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.CREATE_IMAGE
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    command.params["file"] = file_name + "." + file_type
    command.params["size"] = str(size)
    command.params["read_only"] = "false"

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def delete_image(file_name):
    """
    Takes (str) file_name
    Sends a DELETE_IMAGE command to the server
    Returns (dict) with (bool) status and (str) msg
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DELETE_IMAGE
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    command.params["file"] = file_name

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def rename_image(file_name, new_file_name):
    """
    Takes (str) file_name, (str) new_file_name
    Sends a RENAME_IMAGE command to the server
    Returns (dict) with (bool) status and (str) msg
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.RENAME_IMAGE
    command.params["token"] = current_app.config["TOKEN"]
    command.params["locale"] = current_app.config["LOCALE"]

    command.params["from"] = file_name
    command.params["to"] = new_file_name

    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)
    return {"status": result.status, "msg": result.msg}


def delete_file(file_path):
    """
    Takes (str) file_path with the full path to the file to delete
    Returns (dict) with (bool) status and (str) msg
    """
    if os.path.exists(file_path):
        os.remove(file_path)
        return {
                "status": True,
                "msg": _(u"File deleted: %(file_path)s", file_path=file_path),
            }
    return {
            "status": False,
            "msg": _(u"File to delete not found: %(file_path)s", file_path=file_path),
        }


def rename_file(file_path, target_path):
    """
    Takes (str) file_path and (str) target_path
    Returns (dict) with (bool) status and (str) msg
    """
    if os.path.exists(PurePath(target_path).parent):
        os.rename(file_path, target_path)
        return {
            "status": True,
            "msg": _(u"File moved to: %(target_path)s", target_path=target_path),
            }
    return {
        "status": False,
        "msg": _(u"Unable to move file to: %(target_path)s", target_path=target_path),
        }


def unzip_file(file_name, member=False, members=False):
    """
    Takes (str) file_name, optional (str) member, optional (list) of (str) members
    file_name is the name of the zip file to unzip
    member is the full path to the particular file in the zip file to unzip
    members contains all of the full paths to each of the zip archive members
    Returns (dict) with (boolean) status and (list of str) msg
    """
    from asyncio import run
    server_info = get_server_info()
    prop_flag = False

    if not member:
        unzip_proc = run(run_async(
            f"unzip -d {server_info['image_dir']} -n -j "
            f"{server_info['image_dir']}/{file_name}"
            ))
        if members:
            for path in members:
                if path.endswith(PROPERTIES_SUFFIX):
                    name = PurePath(path).name
                    rename_file(f"{server_info['image_dir']}/{name}", f"{CFG_DIR}/{name}")
                    prop_flag = True
    else:
        from re import escape
        member = escape(member)
        unzip_proc = run(run_async(
            f"unzip -d {server_info['image_dir']} -n -j "
            f"{server_info['image_dir']}/{file_name} {member}"
            ))
        # Attempt to unzip a properties file in the same archive dir
        unzip_prop = run(run_async(
            f"unzip -d {CFG_DIR} -n -j "
            f"{server_info['image_dir']}/{file_name} {member}.{PROPERTIES_SUFFIX}"
            ))
        if unzip_prop["returncode"] == 0:
            prop_flag = True
    if unzip_proc["returncode"] != 0:
        logging.warning("Unzipping failed: %s", unzip_proc["stderr"])
        return {"status": False, "msg": unzip_proc["stderr"]}

    from re import findall
    unzipped = findall(
        "(?:inflating|extracting):(.+)\n",
        unzip_proc["stdout"]
        )
    return {"status": True, "msg": unzipped, "prop_flag": prop_flag}


def download_file_to_iso(url, *iso_args):
    """
    Takes (str) url and one or more (str) *iso_args
    Returns (dict) with (bool) status and (str) msg
    """
    from time import time
    from subprocess import run, CalledProcessError
    import asyncio

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

    from zipfile import is_zipfile, ZipFile
    if is_zipfile(tmp_full_path):
        if "XtraStuf.mac" in str(ZipFile(tmp_full_path).namelist()):
            logging.info("MacZip file format detected. Will not unzip to retain resource fork.")
        else:
            logging.info(
                "%s is a zipfile! Will attempt to unzip and store the resulting files.",
                tmp_full_path,
                )
            unzip_proc = asyncio.run(run_async(
                f"unzip -d {tmp_dir} -n {tmp_full_path}"
                ))
            if not unzip_proc["returncode"]:
                logging.info(
                    "%s was successfully unzipped. Deleting the zipfile.",
                    tmp_full_path,
                    )
                delete_file(tmp_full_path)

    try:
        iso_proc = (
            run(
                [
                    "genisoimage",
                    *iso_args,
                    "-o",
                    iso_filename,
                    tmp_dir,
                ],
                capture_output=True,
                check=True,
            )
        )
    except CalledProcessError as error:
        logging.warning("Executed shell command: %s", " ".join(error.cmd))
        logging.warning("Got error: %s", error.stderr.decode("utf-8"))
        return {"status": False, "msg": error.stderr.decode("utf-8")}

    return {
        "status": True,
        "msg": _(u"Created CD-ROM ISO image with arguments \"%(value)s\"", value=" ".join(iso_args)),
        "file_name": iso_filename,
    }


def download_to_dir(url, save_dir):
    """
    Takes (str) url, (str) save_dir
    Returns (dict) with (bool) status and (str) msg
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
    except requests.exceptions.RequestException as error:
        logging.warning("Request failed: %s", str(error))
        return {"status": False, "msg": str(error)}

    logging.info("Response encoding: %s", req.encoding)
    logging.info("Response content-type: %s", req.headers["content-type"])
    logging.info("Response status code: %s", req.status_code)

    return {
        "status": True,
        "msg": _(
            u"%(file_name)s downloaded to %(save_dir)s",
            file_name=file_name,
            save_dir=save_dir,
            ),
        }


def write_config(file_name):
    """
    Takes (str) file_name
    Returns (dict) with (bool) status and (str) msg
    """
    from json import dump
    file_name = f"{CFG_DIR}/{file_name}"
    try:
        with open(file_name, "w") as json_file:
            version = get_server_info()["version"]
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
            reserved_ids_and_memos = []
            reserved_ids = get_reserved_ids()["ids"]
            for scsi_id in reserved_ids:
                reserved_ids_and_memos.append({"id": scsi_id, "memo": RESERVATIONS[int(scsi_id)]})
            dump(
                {"version": version, "devices": devices, "reserved_ids": reserved_ids_and_memos},
                json_file,
                indent=4
                )
        return {"status": True, "msg": _(u"Saved configuration file to %(file_name)s", file_name=file_name)}
    except (IOError, ValueError, EOFError, TypeError) as error:
        logging.error(str(error))
        delete_file(file_name)
        return {"status": False, "msg": str(error)}
    except:
        logging.error("Could not write to file: %s", file_name)
        delete_file(file_name)
        return {
            "status": False,
            "msg": _(u"Could not write to file: %(file_name)s", file_name=file_name),
            }


def read_config(file_name):
    """
    Takes (str) file_name
    Returns (dict) with (bool) status and (str) msg
    """
    from json import load
    file_name = f"{CFG_DIR}/{file_name}"
    try:
        with open(file_name) as json_file:
            config = load(json_file)
            # If the config file format changes again in the future,
            # introduce more sophisticated format detection logic here.
            if isinstance(config, dict):
                detach_all()
                ids_to_reserve = []
                for item in config["reserved_ids"]:
                    ids_to_reserve.append(item["id"])
                    RESERVATIONS[int(item["id"])] = item["memo"]
                reserve_scsi_ids(ids_to_reserve)
                for row in config["devices"]:
                    kwargs = {
                        "device_type": row["device_type"],
                        "image": row["image"],
                        "unit": int(row["unit"]),
                        "vendor": row["vendor"],
                        "product": row["product"],
                        "revision": row["revision"],
                        "block_size": row["block_size"],
                        }
                    params = dict(row["params"])
                    for param in params.keys():
                        kwargs[param] = params[param]
                    attach_image(row["id"], **kwargs)
            # The config file format in RaSCSI 21.10 is using a list data type at the top level.
            # If future config file formats return to the list data type,
            # introduce more sophisticated format detection logic here.
            elif isinstance(config, list):
                detach_all()
                for row in config:
                    kwargs = {
                        "device_type": row["device_type"],
                        "image": row["image"],
                        # "un" for backwards compatibility
                        "unit": int(row["un"]),
                        "vendor": row["vendor"],
                        "product": row["product"],
                        "revision": row["revision"],
                        "block_size": row["block_size"],
                        }
                    params = dict(row["params"])
                    for param in params.keys():
                        kwargs[param] = params[param]
                    attach_image(row["id"], **kwargs)
            else:
                return {"status": False, "msg": _(u"Invalid configuration file format")}
            return {
                "status": True,
                "msg": _(u"Loaded configurations from: %(file_name)s", file_name=file_name),
                }
    except (IOError, ValueError, EOFError, TypeError) as error:
        logging.error(str(error))
        return {"status": False, "msg": str(error)}
    except:
        logging.error("Could not read file: %s", file_name)
        return {
            "status": False,
            "msg": _(u"Could not read configuration file: %(file_name)s", file_name=file_name),
            }


def write_drive_properties(file_name, conf):
    """
    Writes a drive property configuration file to the config dir.
    Takes file name base (str) and (list of dicts) conf as arguments
    Returns (dict) with (bool) status and (str) msg
    """
    from json import dump
    file_path = f"{CFG_DIR}/{file_name}"
    try:
        with open(file_path, "w") as json_file:
            dump(conf, json_file, indent=4)
        return {
            "status": True,
            "msg": _(u"Created properties file: %(file_path)s", file_path=file_path),
            }
    except (IOError, ValueError, EOFError, TypeError) as error:
        logging.error(str(error))
        delete_file(file_path)
        return {"status": False, "msg": str(error)}
    except:
        logging.error("Could not write to file: %s", file_path)
        delete_file(file_path)
        return {
            "status": False,
            "msg": _(u"Could not write to properties file: %(file_path)s", file_path=file_path),
            }


def read_drive_properties(file_path):
    """
    Reads drive properties from json formatted file.
    Takes (str) file_path as argument.
    Returns (dict) with (bool) status, (str) msg, (dict) conf
    """
    from json import load
    try:
        with open(file_path) as json_file:
            conf = load(json_file)
            return {
                "status": True,
                "msg": _(u"Read properties from file: %(file_path)s", file_path=file_path),
                "conf": conf,
                }
    except (IOError, ValueError, EOFError, TypeError) as error:
        logging.error(str(error))
        return {"status": False, "msg": str(error)}
    except:
        logging.error("Could not read file: %s", file_path)
        return {
            "status": False,
            "msg": _(u"Could not read properties from file: %(file_path)s", file_path=file_path),
            }
