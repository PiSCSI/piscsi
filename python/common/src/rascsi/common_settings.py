"""
Module for general settings used in the rascsi module
"""

from os import getcwd

# There may be a more elegant way to get the HOME dir of the user that installed RaSCSI
HOME_DIR = "/".join(getcwd().split("/")[0:3])
CFG_DIR = f"{HOME_DIR}/.config/rascsi"
CONFIG_FILE_SUFFIX = "json"

# File ending used for drive properties files
PROPERTIES_SUFFIX = "properties"

# The RESERVATIONS list is used to keep track of the reserved ID memos.
# Initialize with a list of 8 empty strings.
RESERVATIONS = ["" for x in range(0, 8)]
