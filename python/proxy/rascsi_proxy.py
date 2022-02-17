#! /usr/bin/python

import rascsi_interface_pb2 as proto
import socket
import socket_cmds
import time
from struct import pack, unpack
from yapsy.PluginManager import PluginManager

#########################

def send_pb_command(payload, host='localhost', port=6868):
    """
    Takes a (str) containing a serialized protobuf as argument.
    Establishes a socket connection with RaSCSI.
    """

    counter = 0
    tries = 20
    error_msg = ""

    import socket
    while counter < tries:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.connect((host, port))
                return socket_cmds.send_over_socket(sock, payload)
        except socket.error as error:
            counter += 1
            logging.warning("The RaSCSI service is not responding - attempt %s/%s",
                            str(counter), str(tries))
            error_msg = str(error)
            sleep(0.2)

    logging.error(error_msg)

    # After failing all attempts, throw a 404 error
    abort(404, _(
            u"The RaSCSI Web Interface failed to connect to RaSCSI at %(host)s:%(port)s "
            u"with error: %(error_msg)s. The RaSCSI process is not running or may have crashed.",
            host=host, port=port, error_msg=error_msg,
            )
        )

#########################

def get_image_files_info(host='localhost',port=6868):
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

    data = send_pb_command(command.SerializeToString(),host,port)
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
  
#########################

def socket_read_n(conn, n):
    """ Read exactly n bytes from the socket.
        Raise RuntimeError if the connection closed before
        n bytes were read.
    """
    buf = bytearray(b'')
    while n > 0:
        data = conn.recv(n)
        if data == '':
            raise RuntimeError('unexpected connection close')
        buf.extend(data)
        n -= len(data)
    return bytes(buf)

#########################

def deserialize_message(conn, message):
    # read the header with the size of the protobuf data
    buf = socket_read_n(conn, 4)
    size = (int(buf[3]) << 24) + (int(buf[2]) << 16) + (int(buf[1]) << 8) + int(buf[0])

    buf = socket_read_n(conn, size)

    message.ParseFromString(buf)

#########################

def serialize_message(conn, data):
   conn.sendall(pack("<i", len(data)))
   conn.sendall(data)

#########################

def get_device_id_info(scsi_id, host='localhost', port=6868):
    command = proto.PbCommand()
    command.operation = proto.PbOperation.DEVICES_INFO

    device = proto.PbDeviceDefinition()
    device.id = int(scsi_id)
    command.devices.append(device)

    data = send_pb_command(command.SerializeToString(),host,port)
    result = proto.PbResult()
    result.ParseFromString(data)

    return result


######################################################################
# Main
######################################################################
def main():
    rascsi_host = 'localhost'
    rascsi_port = 6869
    rascsi_proxy_listen_address = '0.0.0.0'
    rascsi_proxy_listen_port = 6868

    ####

    manager = PluginManager()
    manager.setPluginPlaces(["plugins"])
    manager.collectPlugins()
    for plugin in manager.getAllPlugins():
        print("RaSCSI proxy plugin found: " + str(plugin.name))
    
    for plugin in manager.getAllPlugins():
        plugin.plugin_object.init_hook()
    
    address = (rascsi_proxy_listen_address, rascsi_proxy_listen_port)
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind(address)

    server_socket.listen(1)

    while True:
        try:
            connection, client_address = server_socket.accept()
#            print('connection from', client_address)
            buf = socket_read_n(connection, 6)
            magic = buf[0:6]
            if magic != b'RASCSI':
                print("Invalid magic")

            command = proto.PbCommand()
            deserialize_message(connection, command)

            if command.operation == proto.PbOperation.ATTACH:
                images = get_image_files_info(rascsi_host, rascsi_port)

                for device in command.devices:
                    type = proto.PbDeviceType.Name(device.type)
                    filepath = images['images_dir'] + "/" + device.params['file']
#                    print("type: " + str(type))
#                    print("path: " + str(filepath))

                    for plugin in manager.getAllPlugins():
                        plugin.plugin_object.attach_hook(device.id, type, filepath)

                data = send_pb_command(command.SerializeToString(), rascsi_host, rascsi_port)
                serialize_message(connection, data)                       
            elif command.operation == proto.PbOperation.INSERT:
                images = get_image_files_info(rascsi_host, rascsi_port)

                for device in command.devices:
                    type = proto.PbDeviceType.Name(device.type)
                    filepath = images['images_dir'] + "/" + device.params['file']
#                    print("type: " + str(type))
#                    print("path: " + str(filepath))

                    for plugin in manager.getAllPlugins():
                        plugin.plugin_object.insert_hook(device.id, type, filepath)

                data = send_pb_command(command.SerializeToString(), rascsi_host, rascsi_port)
                serialize_message(connection, data)
            elif command.operation == proto.PbOperation.DETACH:
                for device in command.devices:
                    device_details = get_device_id_info(device.id, rascsi_host, rascsi_port)
                    type = proto.PbDeviceType.Name(device_details.devices_info.devices[0].type)
                    filepath = device_details.devices_info.devices[0].file.name
#                    print("type: " + str(type))
#                    print("path: " + str(filepath))

                data = send_pb_command(command.SerializeToString(), rascsi_host, rascsi_port)
                serialize_message(connection, data)
                
                for plugin in manager.getAllPlugins():
                    plugin.plugin_object.detach_hook(device.id, type, filepath)
            elif command.operation == proto.PbOperation.EJECT:
                for device in command.devices:
                    device_details = get_device_id_info(device.id, rascsi_host, rascsi_port)
                    type = proto.PbDeviceType.Name(device_details.devices_info.devices[0].type)
                    filepath = device_details.devices_info.devices[0].file.name
#                    print("type: " + str(type))
#                    print("path: " + str(filepath))

                data = send_pb_command(command.SerializeToString(), rascsi_host, rascsi_port)
                serialize_message(connection, data)
                   
                for plugin in manager.getAllPlugins():
                    plugin.plugin_object.eject_hook(device.id, type, filepath)
            else:
                data = send_pb_command(command.SerializeToString(), rascsi_host, rascsi_port)
                serialize_message(connection, data)
                        
        finally:
             connection.close()


if __name__ == "__main__":
    main()
