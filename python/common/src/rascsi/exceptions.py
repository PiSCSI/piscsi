"""
Module for custom exceptions raised by the rascsi module
"""


class FailedSocketConnectionException(Exception):
    """Raise when a rascsi protobuf socket connection cannot be established after multiple tries."""


class EmptySocketChunkException(Exception):
    """Raise when a socket payload contains an empty chunk which implies a possible problem. """


class InvalidProtobufResponse(Exception):
    """Raise when a rascsi socket payload contains unpexpected data. """
