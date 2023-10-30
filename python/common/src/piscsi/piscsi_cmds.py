"""
Module for commands sent to the PiSCSI backend service.
"""

import logging
from pathlib import PurePath, Path
from functools import lru_cache

import piscsi_interface_pb2 as proto
from piscsi.return_codes import ReturnCodes
from piscsi.socket_cmds import SocketCmds

from piscsi.common_settings import (
    CFG_DIR,
    PROPERTIES_SUFFIX,
    ARCHIVE_FILE_SUFFIXES,
)

from util import unarchiver


class PiscsiCmds:
    """
    Class for commands sent to the PiSCSI backend service.
    """

    def __init__(self, sock_cmd: SocketCmds, token=None, locale="en"):
        self.sock_cmd = sock_cmd
        self.token = token
        self.locale = locale

    def send_pb_command(self, command):
        if logging.getLogger().isEnabledFor(logging.DEBUG):
            logging.debug(self.format_pb_command(command))

        return self.sock_cmd.send_pb_command(command.SerializeToString())

    def list_images(self):
        """
        Sends a IMAGE_FILES_INFO command to the server
        Returns a (dict) with (bool) status, (str) msg, and (list) of (dict)s files
        """
        from piscsi.file_cmds import FileCmds

        self.file_cmd = FileCmds(piscsi=self)

        command = proto.PbCommand()
        command.operation = proto.PbOperation.DEFAULT_IMAGE_FILES_INFO
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)

        server_info = self.get_server_info()
        files = []
        for file in result.image_files_info.image_files:
            prop_file_path = Path(CFG_DIR) / f"{file.name}.{PROPERTIES_SUFFIX}"
            # Add properties meta data for the image, if matching prop file is found
            if prop_file_path.exists():
                process = self.file_cmd.read_drive_properties(prop_file_path)
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

    def get_server_info(self):
        """
        Sends a SERVER_INFO command to the server.
        Returns a dict with:
        - (bool) status
        - (str) version (PiSCSI version number)
        - (list) of (str) log_levels (the log levels PiSCSI supports)
        - (str) current_log_level
        - (list) of (int) reserved_ids
        - (str) image_dir, path to the default images directory
        - (int) scan_depth, the current images directory scan depth
        - 5 distinct (list)s of (str)s with file endings recognized by PiSCSI
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.SERVER_INFO
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        version = (
            str(result.server_info.version_info.major_version)
            + "."
            + str(result.server_info.version_info.minor_version)
            + "."
            + str(result.server_info.version_info.patch_version)
        )
        log_levels = list(result.server_info.log_level_info.log_levels)
        current_log_level = result.server_info.log_level_info.current_log_level
        reserved_ids = list(result.server_info.reserved_ids_info.ids)
        image_dir = result.server_info.image_files_info.default_image_folder
        scan_depth = result.server_info.image_files_info.depth

        # Creates lists of file endings recognized by PiSCSI
        mappings = result.server_info.mapping_info.mapping
        schd = []
        scrm = []
        scmo = []
        sccd = []
        for dtype in mappings:
            if mappings[dtype] == proto.PbDeviceType.SCHD:
                schd.append(dtype)
            elif mappings[dtype] == proto.PbDeviceType.SCRM:
                scrm.append(dtype)
            elif mappings[dtype] == proto.PbDeviceType.SCMO:
                scmo.append(dtype)
            elif mappings[dtype] == proto.PbDeviceType.SCCD:
                sccd.append(dtype)

        return {
            "status": result.status,
            "version": version,
            "log_levels": log_levels,
            "current_log_level": current_log_level,
            "reserved_ids": reserved_ids,
            "image_dir": image_dir,
            "scan_depth": scan_depth,
            "schd": schd,
            "scrm": scrm,
            "scmo": scmo,
            "sccd": sccd,
        }

    def get_reserved_ids(self):
        """
        Sends a RESERVED_IDS_INFO command to the server.
        Returns a dict with:
        - (bool) status
        - (list) of (int) ids -- currently reserved SCSI IDs
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.RESERVED_IDS_INFO
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        scsi_ids = []
        for scsi_id in result.reserved_ids_info.ids:
            scsi_ids.append(str(scsi_id))

        return {"status": result.status, "ids": scsi_ids}

    def get_network_info(self):
        """
        Sends a NETWORK_INTERFACES_INFO command to the server.
        Returns a dict with:
        - (bool) status
        - (list) of (str) ifs (network interfaces detected by PiSCSI)
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.NETWORK_INTERFACES_INFO
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        ifs = result.network_interfaces_info.name
        return {"status": result.status, "ifs": list(ifs)}

    def get_device_types(self):
        """
        Sends a DEVICE_TYPES_INFO command to the server.
        Returns a dict with:
        - (bool) status
        - (dict) device_types, where keys are the four letter device type acronym,
          and the value is a (dict) of supported parameters and their default values.
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.DEVICE_TYPES_INFO
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        device_types = {}
        for device in result.device_types_info.properties:
            params = {}
            for key, value in device.properties.default_params.items():
                params[key] = value
            device_types[proto.PbDeviceType.Name(device.type)] = {
                "removable": device.properties.removable,
                "supports_file": device.properties.supports_file,
                "params": params,
                "block_sizes": list(device.properties.block_sizes),
            }
        return {"status": result.status, "device_types": device_types}

    def get_removable_device_types(self):
        """
        Returns a (list) of (str) of four letter device acronyms
        that are of the removable type.
        """
        device_types = self.get_device_types()
        removable_device_types = []
        for device, value in device_types["device_types"].items():
            if value["removable"]:
                removable_device_types.append(device)
        return removable_device_types

    def get_disk_device_types(self):
        """
        Returns a (list) of (str) of four letter device acronyms
        that take image files as arguments.
        """
        device_types = self.get_device_types()
        disk_device_types = []
        for device, value in device_types["device_types"].items():
            if value["supports_file"]:
                disk_device_types.append(device)
        return disk_device_types

    def get_peripheral_device_types(self):
        """
        Returns a (list) of (str) of four letter device acronyms
        that don't take image files as arguments.
        """
        device_types = self.get_device_types()
        image_device_types = self.get_disk_device_types()
        peripheral_device_types = [
            x for x in device_types["device_types"] if x not in image_device_types
        ]
        return peripheral_device_types

    def get_image_files_info(self):
        """
        Sends a DEFAULT_IMAGE_FILES_INFO command to the server.
        Returns a dict with:
        - (bool) status
        - (str) images_dir, path to images dir
        - (list) of (str) image_files
        - (int) scan_depth, the current scan depth
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.DEFAULT_IMAGE_FILES_INFO
        command.params["token"] = self.token
        command.params["locale"] = self.token

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        images_dir = result.image_files_info.default_image_folder
        image_files = result.image_files_info.image_files
        scan_depth = result.image_files_info.depth
        return {
            "status": result.status,
            "images_dir": images_dir,
            "image_files": image_files,
            "scan_depth": scan_depth,
        }

    def attach_device(self, scsi_id, **kwargs):
        """
        Takes (int) scsi_id and kwargs containing 0 or more device properties

        If the current attached device is a removable device wihout media inserted,
        this sends a INJECT command to the server.
        If there is no currently attached device, this sends the ATTACH command to the server.

        Returns (bool) status and (str) msg

        """
        command = proto.PbCommand()
        command.params["token"] = self.token
        command.params["locale"] = self.locale
        devices = proto.PbDeviceDefinition()
        devices.id = int(scsi_id)

        if kwargs.get("device_type"):
            devices.type = proto.PbDeviceType.Value(str(kwargs["device_type"]))
        if kwargs.get("unit"):
            devices.unit = kwargs["unit"]
        if kwargs.get("params") and isinstance(kwargs["params"], dict):
            for param in kwargs["params"]:
                devices.params[param] = kwargs["params"][param]

        # Handling the inserting of media into an attached removable type device
        device_type = kwargs.get("device_type", None)
        currently_attached = self.list_devices(scsi_id, kwargs.get("unit"))["device_list"]
        if currently_attached:
            current_type = currently_attached[0]["device_type"]
        else:
            current_type = None

        removable_device_types = self.get_removable_device_types()
        if device_type in removable_device_types and current_type in removable_device_types:
            if current_type != device_type:
                parameters = {
                    "device_type": device_type,
                    "current_device_type": current_type,
                }
                return {
                    "status": False,
                    "return_code": ReturnCodes.ATTACHIMAGE_COULD_NOT_ATTACH,
                    "parameters": parameters,
                }
            command.operation = proto.PbOperation.INSERT

        # Handling attaching a new device
        else:
            command.operation = proto.PbOperation.ATTACH
            if kwargs.get("vendor"):
                devices.vendor = kwargs["vendor"]
            if kwargs.get("product"):
                devices.product = kwargs["product"]
            if kwargs.get("revision"):
                devices.revision = kwargs["revision"]
            if kwargs.get("block_size"):
                devices.block_size = int(kwargs["block_size"])

        command.devices.append(devices)

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def detach_by_id(self, scsi_id, unit=None):
        """
        Takes (int) scsi_id and optional (int) unit.
        Sends a DETACH command to the server.
        Returns (bool) status and (str) msg.
        """
        devices = proto.PbDeviceDefinition()
        devices.id = int(scsi_id)
        if unit is not None:
            devices.unit = int(unit)

        command = proto.PbCommand()
        command.operation = proto.PbOperation.DETACH
        command.devices.append(devices)
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def detach_all(self):
        """
        Sends a DETACH_ALL command to the server.
        Returns (bool) status and (str) msg.
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.DETACH_ALL
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def eject_by_id(self, scsi_id, unit=None):
        """
        Takes (int) scsi_id and optional (int) unit.
        Sends an EJECT command to the server.
        Returns (bool) status and (str) msg.
        """
        devices = proto.PbDeviceDefinition()
        devices.id = int(scsi_id)
        if unit is not None:
            devices.unit = int(unit)

        command = proto.PbCommand()
        command.operation = proto.PbOperation.EJECT
        command.devices.append(devices)
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def list_devices(self, scsi_id=None, unit=None):
        """
        Takes optional (int) scsi_id and optional (int) unit.
        Sends a DEVICES_INFO command to the server.
        If no scsi_id is provided, returns a (list) of (dict)s of all attached devices.
        If scsi_id is is provided, returns a (list) of one (dict) for the given device.
        If no attached device is found, returns an empty (list).
        Returns (bool) status, (list) of dicts device_list
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.DEVICES_INFO
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        # If method is called with scsi_id parameter, return the info on those devices
        # Otherwise, return the info on all attached devices
        if scsi_id is not None:
            device = proto.PbDeviceDefinition()
            device.id = int(scsi_id)
            if unit is not None:
                device.unit = int(unit)
            command.devices.append(device)

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)

        device_list = []

        # Return an empty (list) if no devices are attached
        if not result.devices_info.devices:
            return {"status": False, "device_list": []}

        image_files_info = self.get_image_files_info()
        i = 0
        while i < len(result.devices_info.devices):
            did = result.devices_info.devices[i].id
            dunit = result.devices_info.devices[i].unit
            dtype = proto.PbDeviceType.Name(result.devices_info.devices[i].type)
            dstat = result.devices_info.devices[i].status
            dprop = result.devices_info.devices[i].properties

            # Building the status string
            dstat_msg = []
            if dprop.read_only:
                dstat_msg.append("Read-Only")
            if dstat.protected and dprop.protectable:
                dstat_msg.append("Write-Protected")
            if dstat.removed and dprop.removable:
                dstat_msg.append("No Media")
            if dstat.locked and dprop.lockable:
                dstat_msg.append("Locked")

            dpath = result.devices_info.devices[i].file.name
            dfile = dpath.replace(image_files_info["images_dir"] + "/", "")
            dparam = dict(result.devices_info.devices[i].params)
            dven = result.devices_info.devices[i].vendor
            dprod = result.devices_info.devices[i].product
            drev = result.devices_info.devices[i].revision
            dblock = result.devices_info.devices[i].block_size
            dsize = int(result.devices_info.devices[i].block_count) * int(dblock)

            device_list.append(
                {
                    "id": did,
                    "unit": dunit,
                    "device_type": dtype,
                    "status": ", ".join(dstat_msg),
                    "image": dpath,
                    "file": dfile,
                    "params": dparam,
                    "vendor": dven,
                    "product": dprod,
                    "revision": drev,
                    "block_size": dblock,
                    "size": dsize,
                }
            )
            i += 1

        return {"status": result.status, "msg": result.msg, "device_list": device_list}

    def reserve_scsi_ids(self, reserved_scsi_ids):
        """
        Sends the RESERVE_IDS command to the server to reserve SCSI IDs.
        Takes a (list) of (str) as argument.
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.RESERVE_IDS
        command.params["ids"] = ",".join(reserved_scsi_ids)
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def set_log_level(self, log_level):
        """
        Sends a LOG_LEVEL command to the server.
        Takes (str) log_level as an argument.
        Returns (bool) status and (str) msg.
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.LOG_LEVEL
        command.params["level"] = str(log_level)
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def shutdown(self, mode):
        """
        Sends a SHUT_DOWN command to the server.
        Takes (str) mode as an argument.
        The backend will use system calls to reboot or shut down the system.
        It can also shut down the backend process itself.
        Returns (bool) status and (str) msg.
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.SHUT_DOWN
        command.params["mode"] = str(mode)
        command.params["token"] = self.token
        command.params["locale"] = self.locale

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def is_token_auth(self):
        """
        Sends a CHECK_AUTHENTICATION command to the server.
        Tells you whether PiSCSI backend is protected by a token password or not.
        Returns (bool) status and (str) msg.
        """
        command = proto.PbCommand()
        command.operation = proto.PbOperation.CHECK_AUTHENTICATION

        data = self.send_pb_command(command)
        result = proto.PbResult()
        result.ParseFromString(data)
        return {"status": result.status, "msg": result.msg}

    def format_pb_command(self, command):
        """
        Formats the Protobuf command for output
        """
        message = f"Sending: {proto.PbOperation.Name(command.operation)}"

        params = {
            name: "***" if name == "token" else value
            for (name, value) in sorted(command.params.items())
        }
        message += f", params: {params}"

        for device in command.devices:
            formatted_device = {
                key: value
                for (key, value) in {
                    "id": device.id,
                    "unit": device.unit,
                    "type": proto.PbDeviceType.Name(device.type) if device.type else None,
                    "params": device.params,
                    "vendor": device.vendor,
                    "product": device.product,
                    "revision": device.revision,
                }.items()
                if key == "id" or value
            }
            message += f", device: {formatted_device}"

        return message

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
