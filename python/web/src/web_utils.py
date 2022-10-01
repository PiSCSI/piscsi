"""
Module for RaSCSI Web Interface utility methods
"""

import logging
from grp import getgrall
from os import path
from pathlib import Path

from flask import request, make_response
from flask_babel import _
from werkzeug.utils import secure_filename

from rascsi.sys_cmds import SysCmds

def get_valid_scsi_ids(devices, reserved_ids):
    """
    Takes a list of (dict)s devices, and list of (int)s reserved_ids.
    Returns:
    - (list) of (int)s valid_ids, which are the SCSI ids that are not reserved
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
            recommended_id = occupied_ids.pop(0)
        else:
            recommended_id = 0

    return valid_ids, recommended_id


def sort_and_format_devices(devices):
    """
    Takes a (list) of (dict)s devices and returns a (list) of (dict)s.
    Sorts by SCSI ID acending (0 to 7).
    For SCSI IDs where no device is attached, inject a (dict) with placeholder text.
    """
    occupied_ids = []
    for device in devices:
        occupied_ids.append(device["id"])

    formatted_devices = devices

    # Add padding devices and sort the list
    for i in range(8):
        if i not in occupied_ids:
            formatted_devices.append({"id": i, "device_type": "-", \
                    "status": "-", "file": "-", "product": "-"})
    # Sort list of devices by id
    formatted_devices.sort(key=lambda dic: str(dic["id"]))

    return formatted_devices


def map_device_types_and_names(device_types):
    """
    Takes a (dict) corresponding to the data structure returned by RaCtlCmds.get_device_types()
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
        return _("DaynaPORT SCSI/Link")
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


def format_drive_properties(drive_properties):
    """
    Takes a (dict) with structured drive properties data
    Returns a (dict) with the formatted properties, one (list) per device type
    """
    hd_conf = []
    cd_conf = []
    rm_conf = []
    mo_conf = []
    FORMAT_FILTER = "{:,.2f}"

    for device in drive_properties:
        if device["device_type"] == "SCHD":
            device["secure_name"] = secure_filename(device["name"])
            device["size_mb"] = FORMAT_FILTER.format(device["size"] / 1024 / 1024)
            hd_conf.append(device)
        elif device["device_type"] == "SCCD":
            device["size_mb"] = _("N/A")
            cd_conf.append(device)
        elif device["device_type"] == "SCRM":
            device["secure_name"] = secure_filename(device["name"])
            device["size_mb"] = FORMAT_FILTER.format(device["size"] / 1024 / 1024)
            rm_conf.append(device)
        elif device["device_type"] == "SCMO":
            device["secure_name"] = secure_filename(device["name"])
            device["size_mb"] = FORMAT_FILTER.format(device["size"] / 1024 / 1024)
            mo_conf.append(device)

    return {
        "hd_conf": hd_conf,
        "cd_conf": cd_conf,
        "rm_conf": rm_conf,
        "mo_conf": mo_conf,
        }

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
    Returns (bool) False if the network bridge is configured.
    Returns (str) with an error message if the network bridge is not configured.
    """
    sys_cmd = SysCmds()
    if interface.startswith("wlan"):
        if not sys_cmd.introspect_file("/etc/sysctl.conf", r"^net\.ipv4\.ip_forward=1$"):
            return _("Configure IPv4 forwarding before using a wireless network device.")
        if not Path("/etc/iptables/rules.v4").is_file():
            return _("Configure NAT before using a wireless network device.")
    else:
        if not sys_cmd.introspect_file(
                "/etc/dhcpcd.conf",
                r"^denyinterfaces " + interface + r"$",
                ):
            return _("Configure the network bridge before using a wired network device.")
        if not Path("/etc/network/interfaces.d/rascsi_bridge").is_file():
            return _("Configure the network bridge before using a wired network device.")
    return False


def upload_with_dropzonejs(image_dir):
    """
    Takes (str) image_dir which is the path to the image dir to store files.
    Opens a stream to transfer a file via the embedded dropzonejs library.
    """
    log = logging.getLogger("pydrop")
    file_object = request.files["file"]
    file_name = secure_filename(file_object.filename)

    save_path = path.join(image_dir, file_name)
    current_chunk = int(request.form['dzchunkindex'])

    # Makes sure not to overwrite an existing file,
    # but continues writing to a file transfer in progress
    if path.exists(save_path) and current_chunk == 0:
        return make_response(_("The file already exists!"), 400)

    try:
        with open(save_path, "ab") as save:
            save.seek(int(request.form["dzchunkbyteoffset"]))
            save.write(file_object.stream.read())
    except OSError:
        log.exception("Could not write to file")
        return make_response(_("Unable to write the file to disk!"), 500)

    total_chunks = int(request.form["dztotalchunkcount"])

    if current_chunk + 1 == total_chunks:
        # Validate the resulting file size after writing the last chunk
        if path.getsize(save_path) != int(request.form["dztotalfilesize"]):
            log.error(
                    "Finished transferring %s, "
                    "but it has a size mismatch with the original file. "
                    "Got %s but we expected %s.",
                    file_object.filename,
                    path.getsize(save_path),
                    request.form['dztotalfilesize'],
                    )
            return make_response(_("Transferred file corrupted!"), 500)

        log.info("File %s has been uploaded successfully", file_object.filename)
    log.debug("Chunk %s of %s for file %s completed.",
              current_chunk + 1, total_chunks, file_object.filename)

    return make_response(_("File upload successful!"), 200)
