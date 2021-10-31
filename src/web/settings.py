from os import getenv, getcwd

# TODO: Make HOME_DIR portable when running rascsi-web
# as a service, since the HOME env variable doesn't get set then.
HOME_DIR = getenv("HOME", "/home/pi")
CFG_DIR = f"{HOME_DIR}/.config/rascsi/"
AFP_DIR = f"{HOME_DIR}/afpshare"
WEB_DIR = getcwd()

MAX_FILE_SIZE = getenv("MAX_FILE_SIZE", 1024 * 1024 * 1024 * 4)  # 4gb

ARCHIVE_FILE_SUFFIX = "zip"
CONFIG_FILE_SUFFIX = "json"
# File ending used for drive properties files
PROPERTIES_SUFFIX = "properties"

# The file name of the default config file that loads when rascsi-web starts
DEFAULT_CONFIG = f"{CFG_DIR}/default.{CONFIG_FILE_SUFFIX}"
# File containing canonical drive properties
DRIVE_PROPERTIES_FILE = WEB_DIR + "/drive_properties.json"

REMOVABLE_DEVICE_TYPES = ("SCCD", "SCRM", "SCMO")
