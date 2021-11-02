import socket
from struct import pack, unpack

def send_pb_command(payload):
    """
    Takes a str containing a serialized protobuf as argument.
    Establishes a socket connection with RaSCSI.
    """
    # Host and port number where rascsi is listening for socket connections
    HOST = 'localhost'
    PORT = 6868

    counter = 0
    tries = 100
    error_msg = ""

    while counter < tries:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.connect((HOST, PORT))
                return send_over_socket(sock, payload)
        except socket.error as error:
            counter += 1
            print("The RaSCSI service is not responding - attempt %s/%s",
                    str(counter), str(tries))
            error_msg = str(error)

    exit(error_msg)


def send_over_socket(s, payload):
    """
    Takes a socket object and str payload with serialized protobuf.
    Sends payload to RaSCSI over socket and captures the response.
    Tries to extract and interpret the protobuf header to get response size.
    Reads data from socket in 2048 bytes chunks until all data is received.
    """

    # Sending the magic word "RASCSI" to authenticate with the server
    s.send(b"RASCSI")
    # Prepending a little endian 32bit header with the message size
    s.send(pack("<i", len(payload)))
    s.send(payload)

    # Receive the first 4 bytes to get the response header
    response = s.recv(4)
    if len(response) >= 4:
        # Extracting the response header to get the length of the response message
        response_length = unpack("<i", response)[0]
        # Reading in chunks, to handle a case where the response message is very large
        chunks = []
        bytes_recvd = 0
        while bytes_recvd < response_length:
            chunk = s.recv(min(response_length - bytes_recvd, 2048))
            if chunk == b'':
                exit("Socket connection has dropped unexpectedly. "
                     "RaSCSI may have crashed."
                    )
            chunks.append(chunk)
            bytes_recvd = bytes_recvd + len(chunk)
        response_message = b''.join(chunks)
        return response_message
    else:
        exit("The response from RaSCSI did not contain a protobuf header. "
             "RaSCSI may have crashed."
            )
