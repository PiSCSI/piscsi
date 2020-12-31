import os

base_dir = os.getenv('BASE_DIR', "/home/pi/images/")
MAX_FILE_SIZE = os.getenv('MAX_FILE_SIZE', 1024 * 1024 * 1024 * 2)  # 2gb
