"""
Module for return codes that are refrenced in the return payloads of the rascsi module.
"""
from typing import Final


# pylint: disable=too-few-public-methods
class ReturnCodes:
    """Class for the return codes used within the rascsi module."""
    DELETEFILE_SUCCESS: Final = 0
    DELETEFILE_FILE_NOT_FOUND: Final = 1
    RENAMEFILE_SUCCESS: Final = 10
    RENAMEFILE_UNABLE_TO_MOVE: Final = 11
    DOWNLOADFILETOISO_SUCCESS: Final = 20
    DOWNLOADTODIR_SUCCESS: Final = 30
    WRITECONFIG_SUCCESS: Final = 40
    WRITECONFIG_COULD_NOT_WRITE: Final = 41
    READCONFIG_SUCCESS: Final = 50
    READCONFIG_COULD_NOT_READ: Final = 51
    READCONFIG_INVALID_CONFIG_FILE_FORMAT: Final = 51
    WRITEDRIVEPROPS_SUCCESS: Final = 60
    WRITEDRIVEPROPS_COULD_NOT_WRITE: Final = 61
    READDRIVEPROPS_SUCCESS: Final = 70
    READDRIVEPROPS_COULD_NOT_READ: Final = 71
    ATTACHIMAGE_COULD_NOT_ATTACH: Final = 80
