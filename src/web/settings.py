"""
Constant definitions used by other modules
"""

from os import getenv, getcwd

WEB_DIR = getcwd()
# There may be a more elegant way to get the HOME dir of the user that installed RaSCSI
HOME_DIR = "/".join(WEB_DIR.split("/")[0:3])
CFG_DIR = f"{HOME_DIR}/.config/rascsi"
AFP_DIR = f"{HOME_DIR}/afpshare"

MAX_FILE_SIZE = getenv("MAX_FILE_SIZE", str(1024 * 1024 * 1024 * 4))  # 4gb

ARCHIVE_FILE_SUFFIX = "zip"
CONFIG_FILE_SUFFIX = "json"
# File ending used for drive properties files
PROPERTIES_SUFFIX = "properties"

# The file name of the default config file that loads when rascsi-web starts
DEFAULT_CONFIG = f"default.{CONFIG_FILE_SUFFIX}"
# File containing canonical drive properties
DRIVE_PROPERTIES_FILE = WEB_DIR + "/drive_properties.json"

REMOVABLE_DEVICE_TYPES = ("SCCD", "SCRM", "SCMO")

# The RESERVATIONS list is used to keep track of the reserved ID memos.
# Initialize with a list of 8 empty strings.
RESERVATIONS = ["" for x in range(0, 8)]

# The user group that is used for webapp authentication
AUTH_GROUP = "rascsi"
