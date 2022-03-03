"""
Module for return codes that are refrenced in the return payloads of the rascsi module.
"""


# pylint: disable=too-few-public-methods
class ReturnCodes:
    """Class for the return codes used within the rascsi module."""
    DELETEFILE_SUCCESS = 0
    DELETEFILE_FILE_NOT_FOUND = 1
    RENAMEFILE_SUCCESS = 10
    RENAMEFILE_UNABLE_TO_MOVE = 11
    DOWNLOADFILETOISO_SUCCESS = 20
    DOWNLOADTODIR_SUCCESS = 30
    WRITECONFIG_SUCCESS = 40
    WRITECONFIG_COULD_NOT_WRITE = 41
    READCONFIG_SUCCESS = 50
    READCONFIG_COULD_NOT_READ = 51
    READCONFIG_INVALID_CONFIG_FILE_FORMAT = 51
    WRITEDRIVEPROPS_SUCCESS = 60
    WRITEDRIVEPROPS_COULD_NOT_WRITE = 61
    READDRIVEPROPS_SUCCESS = 70
    READDRIVEPROPS_COULD_NOT_READ = 71
    ATTACHIMAGE_COULD_NOT_ATTACH = 80
