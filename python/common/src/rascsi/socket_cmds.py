"""
Module for sending and receiving data over a socket connection with the RaSCSI backend
"""

import logging
from time import sleep
from rascsi.exceptions import (EmptySocketChunkException,
                               InvalidProtobufResponse,
                               FailedSocketConnectionException)


class SocketCmds:
    """
    Class for sending and receiving data over a socket connection with the RaSCSI backend
    """
    def __init__(self, host="localhost", port=6868):
        self.host = host
        self.port = port

    def send_pb_command(self, payload):
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
                    sock.connect((self.host, self.port))
                    response = self.send_over_socket(sock, payload)
                return response
            except socket.error as error:
                counter += 1
                logging.warning("The RaSCSI service is not responding - attempt %s/%s",
                                str(counter), str(tries))
                error_msg = str(error)
                sleep(0.2)
            except EmptySocketChunkException as ex:
                raise ex
            except InvalidProtobufResponse as ex:
                raise ex

        logging.error(error_msg)
        raise FailedSocketConnectionException(error_msg)

    # pylint: disable=no-self-use
    def send_over_socket(self, sock, payload):
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
                    error_message = ("Read an empty chunk from the socket. Socket connection has "
                                     "dropped unexpectedly. RaSCSI may have crashed.")
                    logging.error(error_message)
                    raise EmptySocketChunkException(error_message)
                chunks.append(chunk)
                bytes_recvd = bytes_recvd + len(chunk)
            response_message = b''.join(chunks)
            return response_message

        error_message = ("The response from RaSCSI did not contain a protobuf header. "
                         "RaSCSI may have crashed.")

        logging.error(error_message)
        raise InvalidProtobufResponse(error_message)
