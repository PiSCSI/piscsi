from os import getenv, getcwd

base_dir = getenv("BASE_DIR", "/home/pi/images/")
cfg_dir = getenv("HOME", "/home/pi/") + ".config/rascsi/"
home_dir = getcwd()

DEFAULT_CONFIG = "default.json"
MAX_FILE_SIZE = getenv("MAX_FILE_SIZE", 1024 * 1024 * 1024 * 4)  # 4gb

ARCHIVE_FILE_SUFFIX = ("zip",)

# File containing canonical drive properties
DRIVE_PROPERTIES_FILE = home_dir + "/drive_properties.json"
# File ending used for drive properties files
PROPERTIES_SUFFIX = "properties"

REMOVABLE_DEVICE_TYPES = ("SCCD", "SCRM", "SCMO")
