"""
Module for commands sent to the RaSCSI backend service.
"""
import rascsi_interface_pb2 as proto

def device_list():
    """
    Sends a DEVICES_INFO command to the server.
    Returns a list of dicts with info on all attached devices.
    """
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICES_INFO
    data = send_pb_command(command.SerializeToString())
    result = proto.PbResult()
    result.ParseFromString(data)

    device_list = []
    i = 0

    while i < len(result.devices_info.devices):
        did = result.devices_info.devices[i].id
        dtype = proto.PbDeviceType.Name(result.devices_info.devices[i].type)
        dstat = result.devices_info.devices[i].status
        dprop = result.devices_info.devices[i].properties

        # Building the status string
        dstat_msg = []
        if dstat.protected == True and dprop.protectable == True:
            dstat_msg.append("Write-Protected")
        if dstat.removed == True and dprop.removable == True:
            dstat_msg.append("No Media")
        if dstat.locked == True and dprop.lockable == True:
            dstat_msg.append("Locked")

        dfile = path.basename(result.devices_info.devices[i].file.name)
        dven = result.devices_info.devices[i].vendor
        dprod = result.devices_info.devices[i].product

        device_list.append({
            "id": did,
            "device_type": dtype,
            "status": ", ".join(dstat_msg),
            "file": dfile,
            "vendor": dven,
            "product": dprod,
            })
        i += 1

    return device_list
