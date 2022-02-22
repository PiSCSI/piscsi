"""
Module for RaSCSI Web Interface utility methods
"""

from grp import getgrall
from flask_babel import _


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
        recommended_id = occupied_ids.pop(0)

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
    if device_type == "SAHD":
        return _("SASI Hard Disk")
    if device_type == "SCHD":
        return _("SCSI Hard Disk")
    if device_type == "SCRM":
        return _("Removable Disk")
    if device_type == "SCMO":
        return _("Magneto-Optical")
    if device_type == "SCCD":
        return _("CD / DVD")
    if device_type == "SCBR":
        return _("X68000 Host Bridge")
    if device_type == "SCDP":
        return _("DaynaPORT SCSI/Link")
    if device_type == "SCLP":
        return _("Printer")
    if device_type == "SCHS":
        return _("Host Services")
    return device_type


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
