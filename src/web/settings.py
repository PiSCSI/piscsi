from os import getenv, getcwd

# TODO: Make home_dir portable when running rascsi-web 
# as a service, since the HOME env variable doesn't get set then.
home_dir = getenv("HOME", "/home/pi")
cfg_dir = f"{home_dir}/.config/rascsi/"
afp_dir = f"{home_dir}/afpshare"
web_dir = getcwd()

MAX_FILE_SIZE = getenv("MAX_FILE_SIZE", 1024 * 1024 * 1024 * 4)  # 4gb

ARCHIVE_FILE_SUFFIX = "zip"
CONFIG_FILE_SUFFIX = "json"
# File ending used for drive properties files
PROPERTIES_SUFFIX = "properties"

# The file name of the default config file that loads when rascsi-web starts
DEFAULT_CONFIG = f"default.{CONFIG_FILE_SUFFIX}"
# File containing canonical drive properties
DRIVE_PROPERTIES_FILE = web_dir + "/drive_properties.json"

REMOVABLE_DEVICE_TYPES = ("SCCD", "SCRM", "SCMO")
