"""
Module for PiSCSI Web Interface utility methods
"""

import logging
from grp import getgrall
from pathlib import Path
from ua_parser import user_agent_parser
from re import findall

from flask import request, abort
from flask_babel import _
from werkzeug.utils import secure_filename

from piscsi.sys_cmds import SysCmds


def working_dirs_exist(working_dirs):
    """
    Method for validating that working dirs exist.
    Takes (tuple) of (str) working_dirs with paths to required dirs.
    """
    for dir_path in working_dirs:
        if not Path(dir_path).exists():
            abort(
                503,
                _(f"Please create directory: {dir_path}"),
            )


def get_valid_scsi_ids(devices, reserved_ids):
    """
    Takes a list of (dict)s devices, and list of (int)s reserved_ids.
    Returns:
    - (list) of (int)s valid_ids, which are the SCSI ids that are not reserved
    - (list) of (int)s occupied_ids, which are the SCSI ids were a device is attached
    - (int) recommended_id, which is the id that the Web UI should default to recommend
    """
    occupied_ids = []
    for device in devices:
        occupied_ids.append(device["id"])

    unoccupied_ids = [i for i in list(range(8)) if i not in reserved_ids + occupied_ids]
    unoccupied_ids.sort()
    valid_ids = [i for i in list(range(8)) if i not in reserved_ids]
    valid_ids.sort(reverse=True)

    if unoccupied_ids:
        recommended_id = unoccupied_ids[-1]
    else:
        if occupied_ids:
            recommended_id = occupied_ids[0]
        else:
            recommended_id = 0

    return {
        "valid_ids": valid_ids,
        "occupied_ids": occupied_ids,
        "recommended_id": recommended_id,
    }


def sort_and_format_devices(devices):
    """
    Takes a (list) of (dict)s devices and returns a (list) of (dict)s.
    Sorts by SCSI ID acending (0 to 7).
    For SCSI IDs where no device is attached, inject a (dict) with placeholder text.
    """
    occupied_ids = []
    formatted_devices = []
    for device in devices:
        occupied_ids.append(device["id"])
        device["device_name"] = get_device_name(device["device_type"])
        formatted_devices.append(device)

    # Add placeholder data for non-occupied IDs
    for scsi_id in range(8):
        if scsi_id not in occupied_ids:
            formatted_devices.append(
                {
                    "id": scsi_id,
                    "unit": "-",
                    "device_name": "-",
                    "status": "-",
                    "file": "-",
                    "product": "-",
                }
            )

    return formatted_devices


def map_device_types_and_names(device_types):
    """
    Takes a (dict) corresponding to the data structure returned by PiscsiCmds.get_device_types()
    Returns a (dict) of device_type:device_name mappings of localized device names
    """
    for device in device_types.keys():
        device_types[device]["name"] = get_device_name(device)

    return device_types


# pylint: disable=too-many-return-statements
def get_device_name(device_type):
    """
    Takes a four letter device acronym (str) device_type.
    Returns the human-readable name for the device type.
    """
    if device_type == "SCHD":
        return _("Hard Disk Drive")
    if device_type == "SCRM":
        return _("Removable Disk Drive")
    if device_type == "SCMO":
        return _("Magneto-Optical Drive")
    if device_type == "SCCD":
        return _("CD/DVD Drive")
    if device_type == "SCBR":
        return _("Host Bridge")
    if device_type == "SCDP":
        return _("Ethernet Adapter")
    if device_type == "SCLP":
        return _("Printer")
    if device_type == "SCHS":
        return _("Host Services")
    return device_type


def map_image_file_descriptions(file_suffixes):
    """
    Takes a (list) of (str) file suffixes for images file types.
    Returns a (dict) with file suffix and description pairs, both (str)
    """
    supported_image_types = {}
    for suffix in file_suffixes:
        supported_image_types[suffix] = get_image_description(suffix)

    return supported_image_types


# pylint: disable=too-many-return-statements
def get_image_description(file_suffix):
    """
    Takes a three char file suffix (str) file_suffix.
    Returns the help text description for said file suffix.
    """
    if file_suffix == "hds":
        return _("Hard Disk Image (Generic)")
    if file_suffix == "hda":
        return _("Hard Disk Image (Apple)")
    if file_suffix == "hdn":
        return _("Hard Disk Image (NEC)")
    if file_suffix == "hd1":
        return _("Hard Disk Image (SCSI-1)")
    if file_suffix == "hdr":
        return _("Removable Disk Image")
    if file_suffix == "mos":
        return _("Magneto-Optical Disk Image")
    return file_suffix


def format_image_list(image_files, rootdir, device_types=None):
    """
    Takes:
    - (list) of (dict) image_files
    - (str) rootdir, the name of the images root dir
    - optional (list) device_types
    Returns a formatted (dict) with groups of image_files per subdir key
    """

    root_image_files = []
    subdir_image_files = {}
    for image in image_files:
        if (image["detected_type"] != "UNDEFINED") and device_types:
            image["detected_type_name"] = device_types[image["detected_type"]]["name"]
        subdir_path = findall("^.*/", image["name"])
        if subdir_path:
            subdir = subdir_path[0]
            if f"{rootdir}/{subdir}" in subdir_image_files.keys():
                subdir_image_files[f"{rootdir}/{subdir}"].append(image)
            else:
                subdir_image_files[f"{rootdir}/{subdir}"] = [image]
        else:
            root_image_files.append(image)

    formatted_image_files = dict(sorted(subdir_image_files.items()))
    if root_image_files:
        formatted_image_files[f"{rootdir}/"] = root_image_files
    return formatted_image_files


def format_drive_properties(drive_properties):
    """
    Takes a (dict) with structured drive properties data
    Returns a (dict) with the formatted properties, one (list) per device type
    """
    hd_conf = []
    cd_conf = []
    rm_conf = []
    mo_conf = []

    for device in drive_properties:
        # Fallback for when the properties data is corrupted, to avoid crashing the web app.
        # The integration tests will catch this scenario, but relies on the web app not crashing.
        if not device.get("name"):
            device["name"] = ""

        device["secure_name"] = secure_filename(device["name"])

        if device.get("size"):
            device["size_mb"] = f'{device["size"] / 1024 / 1024:,.2f}'

        if device["device_type"] == "SCHD":
            hd_conf.append(device)
        elif device["device_type"] == "SCCD":
            cd_conf.append(device)
        elif device["device_type"] == "SCRM":
            rm_conf.append(device)
        elif device["device_type"] == "SCMO":
            mo_conf.append(device)

    return {
        "hd_conf": hd_conf,
        "cd_conf": cd_conf,
        "rm_conf": rm_conf,
        "mo_conf": mo_conf,
    }


def get_properties_by_drive_name(drives, drive_name):
    """
    Takes (list) of (dict) drives, and (str) drive_name
    Returns (dict) with the collection of drive properties that matches drive_name
    """
    drives.sort(key=lambda item: item.get("name"))

    for drive in drives:
        if drive["name"] == drive_name:
            return {
                "file_type": drive["file_type"],
                "vendor": drive["vendor"],
                "product": drive["product"],
                "revision": drive["revision"],
                "block_size": drive["block_size"],
                "size": drive["size"],
            }

    logging.error("Properties for drive '%s' does not exist in database", drive_name)
    return False


def auth_active(group):
    """
    Inspects if the group defined in (str) group exists on the system.
    If it exists, tell the webapp to enable authentication.
    Returns a (dict) with (bool) status and (str) msg
    """
    groups = [g.gr_name for g in getgrall()]
    if group in groups:
        return {
            "status": True,
            "msg": _("You must log in to use this function"),
        }
    return {"status": False, "msg": ""}


def is_bridge_configured(interface):
    """
    Takes (str) interface of a network device being attached.
    Returns a (dict) with (bool) status and (str) msg
    """
    PATH_SYSCTL = "/etc/sysctl.conf"
    PATH_IPTV4 = "/etc/iptables/rules.v4"
    PATH_DHCPCD = "/etc/dhcpcd.conf"
    PATH_BRIDGE = "/etc/network/interfaces.d/piscsi_bridge"
    to_configure = []
    sys_cmd = SysCmds()
    if interface.startswith("wlan") or interface.startswith("wlx"):
        return_msg = _("Wireless network bridge enabled for %(interface)s", interface=interface)
        if not sys_cmd.introspect_file(PATH_SYSCTL, r"^net\.ipv4\.ip_forward=1$"):
            to_configure.append("IPv4 forwarding")
        if not Path(PATH_IPTV4).is_file():
            to_configure.append("NAT")
    elif interface.startswith("eth") or interface.startswith("enx"):
        return_msg = _("Wired network bridge enabled for %(interface)s", interface=interface)
        if not sys_cmd.introspect_file(PATH_DHCPCD, r"^denyinterfaces " + interface + r"$"):
            to_configure.append(PATH_DHCPCD)
        if not Path(PATH_BRIDGE).is_file():
            to_configure.append(PATH_BRIDGE)
    else:
        return_msg = _(
            "Unable to detect if %(interface)s is Ethernet or WiFi. "
            "Make sure that the correct network bridge is configured.",
            interface=interface,
        )

    if to_configure:
        return_msg = _(
            "Configure the network bridge for %(interface)s first: ", interface=interface
        )

    return {"status": True, "msg": return_msg + ", ".join(to_configure)}


def is_safe_path(path_name):
    """
    Takes (Path) path_name with the relative path to a file on the file system.
    Returns False if the path is either absolute, or tries to traverse the file system.
    Returns True if neither of the above criteria are met.
    """
    error_message = ""
    if path_name.is_absolute():
        error_message = _("Path must not be absolute")
    elif "../" in str(path_name):
        error_message = _("Path must not traverse the file system")
    elif str(path_name)[0] == "~":
        error_message = _("Path must not start in the home directory")

    if error_message:
        logging.error("Not an allowed path: %s", str(path_name))
        return {"status": False, "msg": error_message}

    return {"status": True, "msg": ""}


def browser_supports_modern_themes():
    """
    Determines if the browser supports the HTML/CSS/JS features used in non-legacy themes.
    """
    user_agent_string = request.headers.get("User-Agent")
    if not user_agent_string:
        return False

    user_agent = user_agent_parser.Parse(user_agent_string)
    if not user_agent["user_agent"]["family"]:
        return False

    # (family, minimum version)
    supported_browsers = [
        ("Safari", 14),
        ("Chrome", 100),
        ("Firefox", 100),
        ("Edge", 100),
        ("Mobile Safari", 14),
        ("Chrome Mobile", 100),
    ]

    current_ua_family = user_agent["user_agent"]["family"]
    current_ua_version = user_agent["user_agent"]["major"]
    logging.info(f"Identified browser as family={current_ua_family}, version={current_ua_version}")

    # Supported browsers cannot be identified without a version
    if not current_ua_version:
        return False

    for supported_browser, supported_version in supported_browsers:
        if (
            current_ua_family == supported_browser
            and float(current_ua_version) >= supported_version
        ):
            return True
    return False
