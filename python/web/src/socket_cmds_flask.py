"""
Module for sending and receiving data over a socket connection with the RaSCSI backend
"""

from flask import abort
from flask_babel import _

from rascsi.exceptions import EmptySocketChunkException, InvalidProtobufResponse, FailedSocketConnectionException
from rascsi.socket_cmds import SocketCmds


class SocketCmdsFlask(SocketCmds):

    def __init__(self, host="localhost", port=6868):
        super().__init__(host, port)

    def send_pb_command(self, payload):
        """
        Takes a (str) containing a serialized protobuf as argument.
        Establishes a socket connection with RaSCSI.
        """
        try:
            return super(SocketCmdsFlask, self).send_pb_command(payload)
        except FailedSocketConnectionException as err:
            # After failing all attempts, throw a 404 error
            abort(404, _(
                u"The RaSCSI Web Interface failed to connect to RaSCSI at %(host)s:%(port)s "
                u"with error: %(error_msg)s. The RaSCSI process is not running or may have crashed.",
                host=self.host, port=self.port, error_msg=str(err),
                )
            )

    def send_over_socket(self, sock, payload):
        try:
            return super(SocketCmdsFlask, self).send_over_socket(sock, payload)
        except EmptySocketChunkException as err:
            abort(
                503, _(
                    u"The RaSCSI Web Interface lost connection to RaSCSI. "
                    u"Please go back and try again. "
                    u"If the issue persists, please report a bug."
                )
            )
        except InvalidProtobufResponse as err:
            abort(
                500, _(
                    u"The RaSCSI Web Interface did not get a valid response from RaSCSI. "
                    u"Please go back and try again. "
                    u"If the issue persists, please report a bug."
                )
            )
