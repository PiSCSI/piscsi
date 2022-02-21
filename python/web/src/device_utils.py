"""
Module for RaSCSI device management utility methods
"""

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
    for key, value in device_types.items():
        if key == "SAHD":
            device_name = _("SASI Hard Disk")
        elif key == "SCHD":
            device_name = _("SCSI Hard Disk")
        elif key == "SCRM":
            device_name = _("Removable Disk")
        elif key == "SCMO":
            device_name = _("Magneto-Optical")
        elif key == "SCCD":
            device_name = _("CD / DVD")
        elif key == "SCBR":
            device_name = _("X68000 Host Bridge")
        elif key == "SCDP":
            device_name = _("DaynaPORT SCSI/Link")
        elif key == "SCLP":
            device_name = _("Printer")
        elif key == "SCHS":
            device_name = _("Host Services")
        else:
            device_name = key
        device_types[key]["name"] = device_name

    return device_types
