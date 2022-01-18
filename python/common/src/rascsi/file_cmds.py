"""
Module for methods reading from and writing to the file system
"""

import os
import logging
from pathlib import PurePath
import asyncio
import rascsi_interface_pb2 as proto
from rascsi.common_settings import CFG_DIR, CONFIG_FILE_SUFFIX, PROPERTIES_SUFFIX, RESERVATIONS
from rascsi.ractl_cmds import RaCtlCmds
from rascsi.return_codes import ReturnCodes
from rascsi.socket_cmds import SocketCmds


class FileCmds:
    """
    class for methods reading from and writing to the file system
    """

    def __init__(self, sock_cmd: SocketCmds, ractl: RaCtlCmds, token=None, locale=None):
        self.sock_cmd = sock_cmd
        self.ractl = ractl
        self.token = token
        self.locale = locale

    # noinspection PyMethodMayBeStatic
    # pylint: disable=no-self-use
    def list_files(self, file_types, dir_path):
        """
        Takes a (list) or (tuple) of (str) file_types - e.g. ('hda', 'hds')
        Returns (list) of (list)s files_list:
        index 0 is (str) file name and index 1 is (int) size in bytes
        """
        files_list = []
        for path, _dirs, files in os.walk(dir_path):
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

    # noinspection PyMethodMayBeStatic
    def list_config_files(self):
        """
        Finds fils with file ending CONFIG_FILE_SUFFIX in CFG_DIR.
        Returns a (list) of (str) files_list
        """
        files_list = []
        for _root, _dirs, files in os.walk(CFG_DIR):
            for file in files:
                if file.endswith("." + CONFIG_FILE_SUFFIX):
                    files_list.append(file)
        return files_list

    def list_images(self):
        """
        Sends a IMAGE_FILES_INFO command to the server
        Returns a (dict) with (bool) status, (str) msg, and (list) of (dict)s files

        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.DEFAULT_IMAGE_FILES_INFO
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.sock_cmd.send_pb_command(command.SerializeToString())
        result = proto.PbResult()
        result.ParseFromString(data)

        # Get a list of all *.properties files in CFG_DIR
        prop_data = self.list_files(PROPERTIES_SUFFIX, CFG_DIR)
        prop_files = [PurePath(x[0]).stem for x in prop_data]

        from zipfile import ZipFile, is_zipfile
        server_info = self.ractl.get_server_info()
        files = []
        for file in result.image_files_info.image_files:
            # Add properties meta data for the image, if applicable
            if file.name in prop_files:
                process = self.read_drive_properties(f"{CFG_DIR}/{file.name}.{PROPERTIES_SUFFIX}")
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

    def create_new_image(self, file_name, file_type, size):
        """
        Takes (str) file_name, (str) file_type, and (int) size
        Sends a CREATE_IMAGE command to the server
        Returns (dict) with (bool) status and (str) msg
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.CREATE_IMAGE
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        command.params["file"] = file_name + "." + file_type
        command.params["size"] = str(size)
        command.params["read_only"] = "false"

        data = self.sock_cmd.send_pb_command(command.SerializeToString())
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def delete_image(self, file_name):
        """
        Takes (str) file_name
        Sends a DELETE_IMAGE command to the server
        Returns (dict) with (bool) status and (str) msg
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.DELETE_IMAGE
        command.params["token"] = self.ractl.token
        command.params["locale"] = self.ractl.locale

        command.params["file"] = file_name

        data = self.sock_cmd.send_pb_command(command.SerializeToString())
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def rename_image(self, file_name, new_file_name):
        """
        Takes (str) file_name, (str) new_file_name
        Sends a RENAME_IMAGE command to the server
        Returns (dict) with (bool) status and (str) msg
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.RENAME_IMAGE
        command.params["token"] = self.ractl.token
        command.params["locale"] = self.ractl.locale

        command.params["from"] = file_name
        command.params["to"] = new_file_name

        data = self.sock_cmd.send_pb_command(command.SerializeToString())
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    # noinspection PyMethodMayBeStatic
    def delete_file(self, file_path):
        """
        Takes (str) file_path with the full path to the file to delete
        Returns (dict) with (bool) status and (str) msg
        """
        parameters = {
            "file_path": file_path
        }
        if os.path.exists(file_path):
            os.remove(file_path)
            return {
                    "status": True,
                    "return_code": ReturnCodes.DELETEFILE_SUCCESS,
                    "parameters": parameters,
                }
        return {
                "status": False,
                "return_code": ReturnCodes.DELETEFILE_FILE_NOT_FOUND,
                "parameters": parameters,
            }

    # noinspection PyMethodMayBeStatic
    def rename_file(self, file_path, target_path):
        """
        Takes (str) file_path and (str) target_path
        Returns (dict) with (bool) status and (str) msg
        """
        parameters = {
            "target_path": target_path
        }
        if os.path.exists(PurePath(target_path).parent):
            os.rename(file_path, target_path)
            return {
                "status": True,
                "return_code": ReturnCodes.RENAMEFILE_SUCCESS,
                "parameters": parameters,
                }
        return {
            "status": False,
            "return_code": ReturnCodes.RENAMEFILE_UNABLE_TO_MOVE,
            "parameters": parameters,
            }

    def unzip_file(self, file_name, member=False, members=False):
        """
        Takes (str) file_name, optional (str) member, optional (list) of (str) members
        file_name is the name of the zip file to unzip
        member is the full path to the particular file in the zip file to unzip
        members contains all of the full paths to each of the zip archive members
        Returns (dict) with (boolean) status and (list of str) msg
        """
        from asyncio import run
        server_info = self.ractl.get_server_info()
        prop_flag = False

        if not member:
            unzip_proc = run(self.run_async(
                f"unzip -d {server_info['image_dir']} -n -j "
                f"{server_info['image_dir']}/{file_name}"
                ))
            if members:
                for path in members:
                    if path.endswith(PROPERTIES_SUFFIX):
                        name = PurePath(path).name
                        self.rename_file(f"{server_info['image_dir']}/{name}", f"{CFG_DIR}/{name}")
                        prop_flag = True
        else:
            from re import escape
            member = escape(member)
            unzip_proc = run(self.run_async(
                f"unzip -d {server_info['image_dir']} -n -j "
                f"{server_info['image_dir']}/{file_name} {member}"
                ))
            # Attempt to unzip a properties file in the same archive dir
            unzip_prop = run(self.run_async(
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

    def download_file_to_iso(self, url, *iso_args):
        """
        Takes (str) url and one or more (str) *iso_args
        Returns (dict) with (bool) status and (str) msg
        """
        from time import time
        from subprocess import run, CalledProcessError

        server_info = self.ractl.get_server_info()

        file_name = PurePath(url).name
        tmp_ts = int(time())
        tmp_dir = "/tmp/" + str(tmp_ts) + "/"
        os.mkdir(tmp_dir)
        tmp_full_path = tmp_dir + file_name
        iso_filename = f"{server_info['image_dir']}/{file_name}.iso"

        req_proc = self.download_to_dir(url, tmp_dir, file_name)

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
                unzip_proc = asyncio.run(self.run_async(
                    f"unzip -d {tmp_dir} -n {tmp_full_path}"
                    ))
                if not unzip_proc["returncode"]:
                    logging.info(
                        "%s was successfully unzipped. Deleting the zipfile.",
                        tmp_full_path,
                        )
                    self.delete_file(tmp_full_path)

        try:
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
        except CalledProcessError as error:
            logging.warning("Executed shell command: %s", " ".join(error.cmd))
            logging.warning("Got error: %s", error.stderr.decode("utf-8"))
            return {"status": False, "msg": error.stderr.decode("utf-8")}

        parameters = {
            "value": " ".join(iso_args)
        }
        return {
            "status": True,
            "return_code": ReturnCodes.DOWNLOADFILETOISO_SUCCESS,
            "parameters": parameters,
            "file_name": iso_filename,
        }

    # noinspection PyMethodMayBeStatic
    def download_to_dir(self, url, save_dir, file_name):
        """
        Takes (str) url, (str) save_dir, (str) file_name
        Returns (dict) with (bool) status and (str) msg
        """
        import requests
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

        parameters = {
            "file_name": file_name,
            "save_dir": save_dir
        }
        return {
            "status": True,
            "return_code": ReturnCodes.DOWNLOADTODIR_SUCCESS,
            "parameters": parameters,
        }

    def write_config(self, file_name):
        """
        Takes (str) file_name
        Returns (dict) with (bool) status and (str) msg
        """
        from json import dump
        file_name = f"{CFG_DIR}/{file_name}"
        try:
            with open(file_name, "w", encoding="ISO-8859-1") as json_file:
                version = self.ractl.get_server_info()["version"]
                devices = self.ractl.list_devices()["device_list"]
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
                reserved_ids = self.ractl.get_reserved_ids()["ids"]
                for scsi_id in reserved_ids:
                    reserved_ids_and_memos.append({"id": scsi_id,
                                                   "memo": RESERVATIONS[int(scsi_id)]})
                dump(
                    {"version": version,
                     "devices": devices,
                     "reserved_ids": reserved_ids_and_memos},
                    json_file,
                    indent=4
                    )
            parameters = {
                "file_name": file_name
            }
            return {
                "status": True,
                "return_code": ReturnCodes.WRITECONFIG_SUCCESS,
                "parameters": parameters,
                }
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
            self.delete_file(file_name)
            return {"status": False, "msg": str(error)}
        except:
            logging.error("Could not write to file: %s", file_name)
            self.delete_file(file_name)
            parameters = {
                "file_name": file_name
            }
            return {
                "status": False,
                "return_code": ReturnCodes.WRITECONFIG_COULD_NOT_WRITE,
                "parameters": parameters,
                }

    def read_config(self, file_name):
        """
        Takes (str) file_name
        Returns (dict) with (bool) status and (str) msg
        """
        from json import load
        file_name = f"{CFG_DIR}/{file_name}"
        try:
            with open(file_name, encoding="ISO-8859-1") as json_file:
                config = load(json_file)
                # If the config file format changes again in the future,
                # introduce more sophisticated format detection logic here.
                if isinstance(config, dict):
                    self.ractl.detach_all()
                    ids_to_reserve = []
                    for item in config["reserved_ids"]:
                        ids_to_reserve.append(item["id"])
                        RESERVATIONS[int(item["id"])] = item["memo"]
                    self.ractl.reserve_scsi_ids(ids_to_reserve)
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
                        self.ractl.attach_image(row["id"], **kwargs)
                # The config file format in RaSCSI 21.10 is using a list data type at the top level.
                # If future config file formats return to the list data type,
                # introduce more sophisticated format detection logic here.
                elif isinstance(config, list):
                    self.ractl.detach_all()
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
                        self.ractl.attach_image(row["id"], **kwargs)
                else:
                    return {"status": False,
                            "return_code": ReturnCodes.READCONFIG_INVALID_CONFIG_FILE_FORMAT}

                parameters = {
                    "file_name": file_name
                }
                return {
                    "status": True,
                    "return_code": ReturnCodes.READCONFIG_SUCCESS,
                    "parameters": parameters
                    }
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
            return {"status": False, "msg": str(error)}
        except:
            logging.error("Could not read file: %s", file_name)
            parameters = {
                "file_name": file_name
            }
            return {
                "status": False,
                "return_code": ReturnCodes.READCONFIG_COULD_NOT_READ,
                "parameters": parameters
                }

    def write_drive_properties(self, file_name, conf):
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
            parameters = {
                "file_path": file_path
            }
            return {
                "status": True,
                "return_code": ReturnCodes.WRITEDRIVEPROPS_SUCCESS,
                "parameters": parameters,
                }
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
            self.delete_file(file_path)
            return {"status": False, "msg": str(error)}
        except:
            logging.error("Could not write to file: %s", file_path)
            self.delete_file(file_path)
            parameters = {
                "file_path": file_path
            }
            return {
                "status": False,
                "return_code": ReturnCodes.WRITEDRIVEPROPS_COULD_NOT_WRITE,
                "parameters": parameters,
                }

    # noinspection PyMethodMayBeStatic
    def read_drive_properties(self, file_path):
        """
        Reads drive properties from json formatted file.
        Takes (str) file_path as argument.
        Returns (dict) with (bool) status, (str) msg, (dict) conf
        """
        from json import load
        try:
            with open(file_path) as json_file:
                conf = load(json_file)
                parameters = {
                    "file_path": file_path
                }
                return {
                    "status": True,
                    "return_codes": ReturnCodes.READDRIVEPROPS_SUCCESS,
                    "parameters": parameters,
                    "conf": conf,
                    }
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
            return {"status": False, "msg": str(error)}
        except:
            logging.error("Could not read file: %s", file_path)
            parameters = {
                "file_path": file_path
            }
            return {
                "status": False,
                "return_codes": ReturnCodes.READDRIVEPROPS_COULD_NOT_READ,
                "parameters": parameters,
                }

    # noinspection PyMethodMayBeStatic
    async def run_async(self, cmd):
        """
        Takes (str) cmd with the shell command to execute
        Executes shell command and captures output
        Returns (dict) with (int) returncode, (str) stdout, (str) stderr
        """
        proc = await asyncio.create_subprocess_shell(
            cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE)

        stdout, stderr = await proc.communicate()

        logging.info("Executed command \"%s\" with status code %d", cmd, proc.returncode)
        if stdout:
            stdout = stdout.decode()
            logging.info("stdout: %s", stdout)
        if stderr:
            stderr = stderr.decode()
            logging.info("stderr: %s", stderr)

        return {"returncode": proc.returncode, "stdout": stdout, "stderr": stderr}
