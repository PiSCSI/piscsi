"""
Module for sending and receiving data over a socket connection with the RaSCSI backend
"""

import logging
from flask import abort
from flask_babel import _
from time import sleep

def send_pb_command(payload):
    """
    Takes a (str) containing a serialized protobuf as argument.
    Establishes a socket connection with RaSCSI.
    """
    # Host and port number where rascsi is listening for socket connections
    host = 'localhost'
    port = 6868

    counter = 0
    tries = 20
    error_msg = ""

    import socket
    while counter < tries:
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.connect((host, port))
                return send_over_socket(sock, payload)
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


def send_over_socket(sock, payload):
    """
    Takes a socket object and (str) payload with serialized protobuf.
    Sends payload to RaSCSI over socket and captures the response.
    Tries to extract and interpret the protobuf header to get response size.
    Reads data from socket in 2048 bytes chunks until all data is received.
    """
    from struct import pack, unpack

    # Sending the magic word "RASCSI" to authenticate with the server
    sock.send(b"RASCSI")
    # Prepending a little endian 32bit header with the message size
    sock.send(pack("<i", len(payload)))
    sock.send(payload)

    # Receive the first 4 bytes to get the response header
    response = sock.recv(4)
    if len(response) >= 4:
        # Extracting the response header to get the length of the response message
        response_length = unpack("<i", response)[0]
        # Reading in chunks, to handle a case where the response message is very large
        chunks = []
        bytes_recvd = 0
        while bytes_recvd < response_length:
            chunk = sock.recv(min(response_length - bytes_recvd, 2048))
            if chunk == b'':
                logging.error(
                    "Read an empty chunk from the socket. "
                    "Socket connection has dropped unexpectedly. "
                    "RaSCSI may have crashed."
                    )
                abort(
                    503, _(
                        u"The RaSCSI Web Interface lost connection to RaSCSI. "
                        u"Please go back and try again. "
                        u"If the issue persists, please report a bug."
                        )
                    )
            chunks.append(chunk)
            bytes_recvd = bytes_recvd + len(chunk)
        response_message = b''.join(chunks)
        return response_message

    logging.error(
        "The response from RaSCSI did not contain a protobuf header. "
        "RaSCSI may have crashed."
        )
    abort(
        500, _(
            u"The RaSCSI Web Interface did not get a valid response from RaSCSI. "
            u"Please go back and try again. "
            u"If the issue persists, please report a bug."
            )
        )
