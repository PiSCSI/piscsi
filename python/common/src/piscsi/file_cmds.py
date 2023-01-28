"""
Module for methods reading from and writing to the file system
"""

import logging
import asyncio
from os import path, walk
from functools import lru_cache
from pathlib import PurePath, Path
from zipfile import ZipFile, is_zipfile
from subprocess import run, Popen, PIPE, CalledProcessError, TimeoutExpired
from json import dump, load
from shutil import copyfile
from urllib.parse import quote
from tempfile import TemporaryDirectory
from re import search

import requests

import piscsi_interface_pb2 as proto
from piscsi.common_settings import (
    CFG_DIR,
    CONFIG_FILE_SUFFIX,
    PROPERTIES_SUFFIX,
    ARCHIVE_FILE_SUFFIXES,
    RESERVATIONS,
    SHELL_ERROR,
)
from piscsi.piscsi_cmds import PiscsiCmds
from piscsi.return_codes import ReturnCodes
from piscsi.socket_cmds import SocketCmds
from util import unarchiver

FILE_READ_ERROR = "Unhandled exception when reading file: %s"
FILE_WRITE_ERROR = "Unhandled exception when writing to file: %s"
URL_SAFE = "/:?&"


class FileCmds:
    """
    class for methods reading from and writing to the file system
    """

    def __init__(self, sock_cmd: SocketCmds, piscsi: PiscsiCmds, token=None, locale=None):
        self.sock_cmd = sock_cmd
        self.piscsi = piscsi
        self.token = token
        self.locale = locale

    def send_pb_command(self, command):
        if logging.getLogger().isEnabledFor(logging.DEBUG):
            # TODO: Uncouple/move to common dependency
            logging.debug(self.piscsi.format_pb_command(command))

        return self.sock_cmd.send_pb_command(command.SerializeToString())

    # noinspection PyMethodMayBeStatic
    def list_config_files(self):
        """
        Finds fils with file ending CONFIG_FILE_SUFFIX in CFG_DIR.
        Returns a (list) of (str) files_list
        """
        files_list = []
        for _root, _dirs, files in walk(CFG_DIR):
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

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)

        server_info = self.piscsi.get_server_info()
        files = []
        for file in result.image_files_info.image_files:
            prop_file_path = Path(CFG_DIR) / f"{file.name}.{PROPERTIES_SUFFIX}"
            # Add properties meta data for the image, if matching prop file is found
            if prop_file_path.exists():
                process = self.read_drive_properties(prop_file_path)
                prop = process["conf"]
            else:
                prop = False

            archive_contents = []
            if PurePath(file.name).suffix.lower()[1:] in ARCHIVE_FILE_SUFFIXES:
                try:
                    archive_info = self._get_archive_info(
                        f"{server_info['image_dir']}/{file.name}",
                        _cache_extra_key=file.size,
                    )

                    properties_files = [
                        x["path"]
                        for x in archive_info["members"]
                        if x["path"].endswith(PROPERTIES_SUFFIX)
                    ]

                    for member in archive_info["members"]:
                        if member["is_dir"] or member["is_resource_fork"]:
                            continue

                        if PurePath(member["path"]).suffix.lower()[1:] == PROPERTIES_SUFFIX:
                            member["is_properties_file"] = True
                        elif f"{member['path']}.{PROPERTIES_SUFFIX}" in properties_files:
                            member[
                                "related_properties_file"
                            ] = f"{member['path']}.{PROPERTIES_SUFFIX}"

                        archive_contents.append(member)
                except (unarchiver.LsarCommandError, unarchiver.LsarOutputError):
                    pass

            size_mb = "{:,.1f}".format(file.size / 1024 / 1024)
            dtype = proto.PbDeviceType.Name(file.type)
            files.append(
                {
                    "name": file.name,
                    "size": file.size,
                    "size_mb": size_mb,
                    "detected_type": dtype,
                    "prop": prop,
                    "archive_contents": archive_contents,
                }
            )

        return {"status": result.status, "msg": result.msg, "files": files}

    # noinspection PyMethodMayBeStatic
    def delete_file(self, file_path):
        """
        Takes (Path) file_path for the file to delete
        Returns (dict) with (bool) status, (str) msg, (dict) parameters
        """
        parameters = {"file_path": file_path}

        if file_path.exists():
            file_path.unlink()
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
        Takes:
         - (Path) file_path for the file to rename
         - (Path) target_path for the name to rename
        Returns (dict) with (bool) status, (str) msg, (dict) parameters
        """
        parameters = {"target_path": target_path}
        if not target_path.parent.exists():
            target_path.parent.mkdir(parents=True)
        if target_path.parent.exists() and not target_path.exists():
            file_path.rename(target_path)
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

    # noinspection PyMethodMayBeStatic
    def copy_file(self, file_path, target_path):
        """
        Takes:
         - (Path) file_path for the file to copy from
         - (Path) target_path for the name to copy to
        Returns (dict) with (bool) status, (str) msg, (dict) parameters
        """
        parameters = {"target_path": target_path}
        if not target_path.parent.exists():
            target_path.parent.mkdir(parents=True)
        if target_path.parent.exists() and not target_path.exists():
            copyfile(str(file_path), str(target_path))
            return {
                "status": True,
                "return_code": ReturnCodes.WRITEFILE_SUCCESS,
                "parameters": parameters,
            }
        return {
            "status": False,
            "return_code": ReturnCodes.WRITEFILE_COULD_NOT_WRITE,
            "parameters": parameters,
        }

    def create_empty_image(self, target_path, size):
        """
        Takes (Path) target_path and (int) size in bytes
        Creates a new empty binary file to use as image
        Returns (dict) with (bool) status, (str) msg, (dict) parameters
        """
        parameters = {"target_path": target_path}
        if not target_path.parent.exists():
            target_path.parent.mkdir(parents=True)
        if target_path.parent.exists() and not target_path.exists():
            try:
                with open(f"{target_path}", "wb") as out:
                    out.seek(size - 1)
                    out.write(b"\0")
            except OSError as error:
                return {"status": False, "msg": str(error)}

            return {"status": True, "msg": ""}

        return {
            "status": False,
            "return_code": ReturnCodes.WRITEFILE_COULD_NOT_WRITE,
            "parameters": parameters,
        }

    def extract_image(self, file_path, members=None, move_properties_files_to_config=True):
        """
        Takes (str) file_path, (list) members, optional (bool) move_properties_files_to_config
        file_name is the path of the archive file to extract, relative to the images directory
        members is a list of file paths in the archive file to extract
        move_properties_files_to_config controls if .properties files are auto-moved to CFG_DIR
        Returns (dict) result
        """
        server_info = self.piscsi.get_server_info()

        if not members:
            return {
                "status": False,
                "return_code": ReturnCodes.EXTRACTIMAGE_NO_FILES_SPECIFIED,
            }

        try:
            extract_result = unarchiver.extract_archive(
                f"{server_info['image_dir']}/{file_path}",
                members=members,
                output_dir=server_info["image_dir"],
            )

            properties_files_moved = []
            if move_properties_files_to_config:
                for file in extract_result["extracted"]:
                    if file.get("name").endswith(f".{PROPERTIES_SUFFIX}"):
                        prop_path = Path(CFG_DIR) / file["name"]
                        if self.rename_file(
                            Path(file["absolute_path"]),
                            prop_path,
                        ):
                            properties_files_moved.append(
                                {
                                    "status": True,
                                    "name": file["path"],
                                    "path": str(prop_path),
                                }
                            )
                        else:
                            properties_files_moved.append(
                                {
                                    "status": False,
                                    "name": file["path"],
                                    "path": str(prop_path),
                                }
                            )

            return {
                "status": True,
                "return_code": ReturnCodes.EXTRACTIMAGE_SUCCESS,
                "parameters": {
                    "count": len(extract_result["extracted"]),
                },
                "extracted": extract_result["extracted"],
                "skipped": extract_result["skipped"],
                "properties_files_moved": properties_files_moved,
            }
        except unarchiver.UnarNoFilesExtractedError:
            return {
                "status": False,
                "return_code": ReturnCodes.EXTRACTIMAGE_NO_FILES_EXTRACTED,
            }
        except (
            unarchiver.UnarCommandError,
            unarchiver.UnarUnexpectedOutputError,
        ) as error:
            return {
                "status": False,
                "return_code": ReturnCodes.EXTRACTIMAGE_COMMAND_ERROR,
                "parameters": {
                    "error": error,
                },
            }

    # noinspection PyMethodMayBeStatic
    def partition_disk(self, file_name, volume_name, disk_format):
        """
        Creates a partition table on an image file.
        Takes (str) file_name, (str) volume_name, (str) disk_format as arguments.
        disk_format is either HFS or FAT
        Returns (dict) with (bool) status, (str) msg
        """
        server_info = self.piscsi.get_server_info()
        full_file_path = Path(server_info["image_dir"]) / file_name

        # Inject hfdisk commands to create Drive with correct partitions
        # https://www.codesrc.com/mediawiki/index.php/HFSFromScratch
        # i                         initialize partition map
        # continue with default first block
        # C                         Create 1st partition with type specified next)
        # continue with default
        # 32                        32 blocks (required for HFS+)
        # Driver_Partition          Partition Name
        # Apple_Driver              Partition Type  (available types: Apple_Driver,
        #                           Apple_Driver43, Apple_Free, Apple_HFS...)
        # C                         Create 2nd partition with type specified next
        # continue with default first block
        # continue with default block size (rest of the disk)
        # ${volumeName}             Partition name provided by user
        # Apple_HFS                 Partition Type
        # w                         Write partition map to disk
        # y                         Confirm partition table
        # p                         Print partition map
        if disk_format == "HFS":
            partitioning_tool = "hfdisk"
            commands = [
                "i",
                "",
                "C",
                "",
                "32",
                "Driver_Partition",
                "Apple_Driver",
                "C",
                "",
                "",
                volume_name,
                "Apple_HFS",
                "w",
                "y",
                "p",
            ]
        # Create a DOS label, primary partition, W95 FAT type
        elif disk_format == "FAT":
            partitioning_tool = "fdisk"
            commands = [
                "o",
                "n",
                "p",
                "",
                "",
                "",
                "t",
                "b",
                "w",
            ]
        try:
            process = Popen(
                [partitioning_tool, str(full_file_path)],
                stdin=PIPE,
                stdout=PIPE,
            )
            for command in commands:
                process.stdin.write(bytes(command + "\n", "utf-8"))
                process.stdin.flush()
            try:
                outs, errs = process.communicate(timeout=15)
                if outs:
                    logging.info(str(outs, "utf-8"))
                if errs:
                    logging.error(str(errs, "utf-8"))
                if process.returncode:
                    self.delete_file(Path(file_name))
                    return {"status": False, "msg": errs}
            except TimeoutExpired:
                process.kill()
                outs, errs = process.communicate()
                if outs:
                    logging.info(str(outs, "utf-8"))
                if errs:
                    logging.error(str(errs, "utf-8"))
                self.delete_file(Path(file_name))
                return {"status": False, "msg": errs}

        except (OSError, IOError) as error:
            logging.error(SHELL_ERROR, " ".join(error.cmd), error.stderr.decode("utf-8"))
            self.delete_file(Path(file_name))
            return {"status": False, "msg": error.stderr.decode("utf-8")}

        return {"status": True, "msg": ""}

    # noinspection PyMethodMayBeStatic
    def format_hfs(self, file_name, volume_name, driver_path):
        """
        Initializes an HFS file system and injects a hard disk driver
        Takes (str) file_name, (str) volume_name and (Path) driver_path as arguments.
        Returns (dict) with (bool) status, (str) msg
        """
        server_info = self.piscsi.get_server_info()
        full_file_path = Path(server_info["image_dir"]) / file_name

        try:
            run(
                [
                    "dd",
                    f"if={driver_path}",
                    f"of={full_file_path}",
                    "seek=64",
                    "count=32",
                    "bs=512",
                    "conv=notrunc",
                ],
                capture_output=True,
                check=True,
            )
        except (FileNotFoundError, CalledProcessError) as error:
            logging.warning(SHELL_ERROR, " ".join(error.cmd), error.stderr.decode("utf-8"))
            self.delete_file(Path(file_name))
            return {"status": False, "msg": error.stderr.decode("utf-8")}

        try:
            process = run(
                [
                    "hformat",
                    "-l",
                    volume_name,
                    str(full_file_path),
                    "1",
                ],
                capture_output=True,
                check=True,
            )
            logging.info(process.stdout.decode("utf-8"))
        except (FileNotFoundError, CalledProcessError) as error:
            logging.error(SHELL_ERROR, " ".join(error.cmd), error.stderr.decode("utf-8"))
            self.delete_file(Path(file_name))
            return {"status": False, "msg": error.stderr.decode("utf-8")}

        return {"status": True, "msg": ""}

    # noinspection PyMethodMayBeStatic
    def format_fat(self, file_name, volume_name, fat_size):
        """
        Initializes a FAT file system
        Takes (str) file_name, (str) volume_name and (str) FAT size (12|16|32) as arguments.
        Returns (dict) with (bool) status, (str) msg
        """
        server_info = self.piscsi.get_server_info()
        full_file_path = Path(server_info["image_dir"]) / file_name
        loopback_device = ""

        try:
            process = run(
                ["kpartx", "-av", str(full_file_path)],
                capture_output=True,
                check=True,
            )
            logging.info(process.stdout.decode("utf-8"))
            if process.returncode == 0:
                loopback_device = search(r"(loop\d\D\d)", process.stdout.decode("utf-8")).group(1)
            else:
                logging.info(process.stdout.decode("utf-8"))
                self.delete_file(Path(file_name))
                return {"status": False, "msg": process.stderr.decode("utf-8")}
        except (FileNotFoundError, CalledProcessError) as error:
            logging.warning(SHELL_ERROR, " ".join(error.cmd), error.stderr.decode("utf-8"))
            self.delete_file(Path(file_name))
            return {"status": False, "msg": error.stderr.decode("utf-8")}

        args = [
            "mkfs.fat",
            "-v",
            "-F",
            fat_size,
            "-n",
            volume_name,
            "/dev/mapper/" + loopback_device,
        ]
        try:
            process = run(
                args,
                capture_output=True,
                check=True,
            )
            logging.info(process.stdout.decode("utf-8"))
        except (FileNotFoundError, CalledProcessError) as error:
            logging.warning(SHELL_ERROR, " ".join(error.cmd), error.stderr.decode("utf-8"))
            self.delete_file(Path(file_name))
            return {"status": False, "msg": error.stderr.decode("utf-8")}

        try:
            process = run(
                ["kpartx", "-dv", str(full_file_path)],
                capture_output=True,
                check=True,
            )
            logging.info(process.stdout.decode("utf-8"))
            if process.returncode:
                logging.info(process.stderr.decode("utf-8"))
                logging.warning("Failed to delete loopback device. You may have to do it manually")
        except (FileNotFoundError, CalledProcessError) as error:
            logging.warning(SHELL_ERROR, " ".join(error.cmd), error.stderr.decode("utf-8"))
            self.delete_file(Path(file_name))
            return {"status": False, "msg": error.stderr.decode("utf-8")}

        return {"status": True, "msg": ""}

    def download_file_to_iso(self, url, *iso_args):
        """
        Takes (str) url and one or more (str) *iso_args
        Returns (dict) with (bool) status and (str) msg
        """

        server_info = self.piscsi.get_server_info()

        file_name = PurePath(url).name
        iso_filename = Path(server_info["image_dir"]) / f"{file_name}.iso"

        with TemporaryDirectory() as tmp_dir:
            req_proc = self.download_to_dir(quote(url, safe=URL_SAFE), tmp_dir, file_name)
            logging.info("Downloaded %s to %s", file_name, tmp_dir)
            if not req_proc["status"]:
                return {"status": False, "msg": req_proc["msg"]}

            tmp_full_path = Path(tmp_dir) / file_name
            if is_zipfile(tmp_full_path):
                if "XtraStuf.mac" in str(ZipFile(str(tmp_full_path)).namelist()):
                    logging.info(
                        "MacZip file format detected. Will not unzip to retain resource fork."
                    )
                else:
                    logging.info(
                        "%s is a zipfile! Will attempt to unzip and store the resulting files.",
                        tmp_full_path,
                    )
                    unzip_proc = asyncio.run(
                        self.run_async(
                            "unzip",
                            [
                                "-d",
                                str(tmp_dir),
                                "-n",
                                str(tmp_full_path),
                            ],
                        )
                    )
                    if not unzip_proc["returncode"]:
                        logging.info(
                            "%s was successfully unzipped. Deleting the zipfile.",
                            tmp_full_path,
                        )
                        tmp_full_path.unlink(True)

            process = self.generate_iso(iso_filename, Path(tmp_dir), *iso_args)

            if not process["status"]:
                return {"status": False, "msg": process["msg"]}

            return {
                "status": True,
                "return_code": process["return_code"],
                "parameters": process["parameters"],
                "file_name": process["file_name"],
            }

    def generate_iso(self, iso_file, target_path, *iso_args):
        """
        Takes
        - (Path) iso_file - the path to the file to create
        - (Path) target_path - the path to the file or dir to generate the iso from
        - (*str) iso_args - the tuple of arguments to pass to genisoimage
        """
        try:
            run(
                [
                    "genisoimage",
                    *iso_args,
                    "-o",
                    str(iso_file),
                    str(target_path),
                ],
                capture_output=True,
                check=True,
            )
        except CalledProcessError as error:
            logging.warning(SHELL_ERROR, " ".join(error.cmd), error.stderr.decode("utf-8"))
            return {"status": False, "msg": error.stderr.decode("utf-8")}

        return {
            "status": True,
            "return_code": ReturnCodes.DOWNLOADFILETOISO_SUCCESS,
            "parameters": {"value": " ".join(iso_args)},
            "file_name": iso_file.name,
        }

    # noinspection PyMethodMayBeStatic
    def download_to_dir(self, url, save_dir, file_name):
        """
        Takes (str) url, (str) save_dir, (str) file_name
        Returns (dict) with (bool) status and (str) msg
        """
        logging.info("Making a request to download %s", url)

        try:
            with requests.get(
                quote(url, safe=URL_SAFE),
                stream=True,
                headers={"User-Agent": "Mozilla/5.0"},
            ) as req:
                req.raise_for_status()
                try:
                    with open(f"{save_dir}/{file_name}", "wb") as download:
                        for chunk in req.iter_content(chunk_size=8192):
                            download.write(chunk)
                except FileNotFoundError as error:
                    return {"status": False, "msg": str(error)}
        except requests.exceptions.RequestException as error:
            logging.warning("Request failed: %s", str(error))
            return {"status": False, "msg": str(error)}

        logging.info("Response encoding: %s", req.encoding)
        logging.info("Response content-type: %s", req.headers["content-type"])
        logging.info("Response status code: %s", req.status_code)

        parameters = {"file_name": file_name, "save_dir": save_dir}
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
        file_path = f"{CFG_DIR}/{file_name}"
        try:
            with open(file_path, "w", encoding="ISO-8859-1") as json_file:
                version = self.piscsi.get_server_info()["version"]
                devices = self.piscsi.list_devices()["device_list"]
                for device in devices:
                    # Remove keys that we don't want to store in the file
                    del device["status"]
                    del device["file"]
                    # It's cleaner not to store an empty parameter for every device without media
                    if device["image"] == "":
                        device["image"] = None
                    # PiSCSI product names will be generated on the fly by PiSCSI
                    if device["vendor"] == "PiSCSI":
                        device["vendor"] = device["product"] = device["revision"] = None
                    # A block size of 0 is how PiSCSI indicates N/A for block size
                    if device["block_size"] == 0:
                        device["block_size"] = None
                    # Convert to a data type that can be serialized
                    device["params"] = dict(device["params"])
                reserved_ids_and_memos = []
                reserved_ids = self.piscsi.get_reserved_ids()["ids"]
                for scsi_id in reserved_ids:
                    reserved_ids_and_memos.append(
                        {"id": scsi_id, "memo": RESERVATIONS[int(scsi_id)]}
                    )
                dump(
                    {
                        "version": version,
                        "devices": devices,
                        "reserved_ids": reserved_ids_and_memos,
                    },
                    json_file,
                    indent=4,
                )
            parameters = {"target_path": file_path}
            return {
                "status": True,
                "return_code": ReturnCodes.WRITEFILE_SUCCESS,
                "parameters": parameters,
            }
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
            self.delete_file(Path(file_path))
            return {"status": False, "msg": str(error)}
        except Exception:
            logging.error(FILE_WRITE_ERROR, file_name)
            self.delete_file(Path(file_path))
            raise

    def read_config(self, file_name):
        """
        Takes (str) file_name
        Returns (dict) with (bool) status and (str) msg
        """
        file_path = Path(CFG_DIR) / file_name
        try:
            with open(file_path, encoding="ISO-8859-1") as json_file:
                config = load(json_file)
                # If the config file format changes again in the future,
                # introduce more sophisticated format detection logic here.
                if isinstance(config, dict):
                    self.piscsi.detach_all()
                    for scsi_id in range(0, 8):
                        RESERVATIONS[scsi_id] = ""
                    ids_to_reserve = []
                    for item in config["reserved_ids"]:
                        ids_to_reserve.append(item["id"])
                        RESERVATIONS[int(item["id"])] = item["memo"]
                    self.piscsi.reserve_scsi_ids(ids_to_reserve)
                    for row in config["devices"]:
                        kwargs = {
                            "device_type": row["device_type"],
                            "unit": int(row["unit"]),
                            "vendor": row["vendor"],
                            "product": row["product"],
                            "revision": row["revision"],
                            "block_size": row["block_size"],
                            "params": dict(row["params"]),
                        }
                        if row["image"]:
                            kwargs["params"]["file"] = row["image"]
                        self.piscsi.attach_device(row["id"], **kwargs)
                # The config file format in RaSCSI 21.10 is using a list data type at the top level.
                # If future config file formats return to the list data type,
                # introduce more sophisticated format detection logic here.
                elif isinstance(config, list):
                    self.piscsi.detach_all()
                    for row in config:
                        kwargs = {
                            "device_type": row["device_type"],
                            "image": row["image"],
                            "unit": int(row["un"]),
                            "vendor": row["vendor"],
                            "product": row["product"],
                            "revision": row["revision"],
                            "block_size": row["block_size"],
                            "params": dict(row["params"]),
                        }
                        if row["image"]:
                            kwargs["params"]["file"] = row["image"]
                        self.piscsi.attach_device(row["id"], **kwargs)
                    logging.warning("%s is in an obsolete config file format", file_name)
                else:
                    return {
                        "status": False,
                        "return_code": ReturnCodes.READCONFIG_INVALID_CONFIG_FILE_FORMAT,
                    }

                parameters = {"file_name": file_name}
                return {
                    "status": True,
                    "return_code": ReturnCodes.READCONFIG_SUCCESS,
                    "parameters": parameters,
                }
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
            return {"status": False, "msg": str(error)}
        except Exception:
            logging.error(FILE_READ_ERROR, str(file_path))
            raise

    def write_drive_properties(self, file_name, conf):
        """
        Writes a drive property configuration file to the config dir.
        Takes file name base (str) and (list of dicts) conf as arguments
        Returns (dict) with (bool) status and (str) msg
        """
        file_path = Path(CFG_DIR) / file_name
        if not file_path.parent.exists():
            file_path.parent.mkdir(parents=True)
        try:
            with open(file_path, "w") as json_file:
                dump(conf, json_file, indent=4)
            parameters = {"target_path": str(file_path)}
            return {
                "status": True,
                "return_code": ReturnCodes.WRITEFILE_SUCCESS,
                "parameters": parameters,
            }
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
            self.delete_file(file_path)
            return {"status": False, "msg": str(error)}
        except Exception:
            logging.error(FILE_WRITE_ERROR, str(file_path))
            self.delete_file(file_path)
            raise

    # noinspection PyMethodMayBeStatic
    def read_drive_properties(self, file_path):
        """
        Reads drive properties from json formatted file.
        Takes (Path) file_path as argument.
        Returns (dict) with (bool) status, (str) msg, (dict) conf
        """
        try:
            with open(file_path) as json_file:
                conf = load(json_file)
                parameters = {"file_path": str(file_path)}
                return {
                    "status": True,
                    "return_codes": ReturnCodes.READDRIVEPROPS_SUCCESS,
                    "parameters": parameters,
                    "conf": conf,
                }
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
            return {"status": False, "msg": str(error)}
        except Exception:
            logging.error(FILE_READ_ERROR, str(file_path))
            raise

    # noinspection PyMethodMayBeStatic
    async def run_async(self, program, args):
        """
        Takes (str) cmd with the shell command to execute
        Executes shell command and captures output
        Returns (dict) with (int) returncode, (str) stdout, (str) stderr
        """
        proc = await asyncio.create_subprocess_exec(
            program,
            *args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )

        stdout, stderr = await proc.communicate()

        logging.info(
            'Executed command "%s %s" with status code %d',
            program,
            " ".join(args),
            proc.returncode,
        )
        if stdout:
            stdout = stdout.decode()
            logging.info("stdout: %s", stdout)
        if stderr:
            stderr = stderr.decode()
            logging.info("stderr: %s", stderr)

        return {"returncode": proc.returncode, "stdout": stdout, "stderr": stderr}

    # noinspection PyMethodMayBeStatic
    @lru_cache(maxsize=32)
    def _get_archive_info(self, file_path, **kwargs):
        """
        Cached wrapper method to improve performance, e.g. on index screen
        """
        try:
            return unarchiver.inspect_archive(file_path)
        except (unarchiver.LsarCommandError, unarchiver.LsarOutputError) as error:
            logging.error(str(error))
            raise
