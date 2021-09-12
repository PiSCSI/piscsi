from os import getenv

base_dir = getenv("BASE_DIR", "/home/pi/images/")
DEFAULT_CONFIG = base_dir + "default.json"
MAX_FILE_SIZE = getenv("MAX_FILE_SIZE", 1024 * 1024 * 1024 * 2)  # 2gb

CDROM_FILE_SUFFIX = (".iso", ".cdr", ".toast", ".img")
REMOVABLE_FILE_SUFFIX = (".hdr", )
VALID_FILE_SUFFIX = ["*.hda", "*.hdn", "*.hdi", "*.nhd", "*.hdf", "*.hds", \
        "*.hdr", "*.iso", "*.cdr", "*.toast", "*.img", "*.zip"]
