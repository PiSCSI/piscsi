"""
Module for RaSCSI device management utility methods
"""

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
