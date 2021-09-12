from os import getenv

base_dir = getenv("BASE_DIR", "/home/pi/images/")
MAX_FILE_SIZE = getenv("MAX_FILE_SIZE", 1024 * 1024 * 1024 * 2)  # 2gb
