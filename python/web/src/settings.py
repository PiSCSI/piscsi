"""
Constant definitions used by other modules
"""

from os import getenv, getcwd
import piscsi.common_settings

WEB_DIR = getcwd()
HOME_DIR = "/".join(WEB_DIR.split("/")[0:3])

FILE_SERVER_DIR = f"{HOME_DIR}/shared_files"

MAX_FILE_SIZE = getenv("MAX_FILE_SIZE", str(1024 * 1024 * 1024 * 4))  # 4gb

# The file name of the default config file that loads when piscsi-web starts
DEFAULT_CONFIG = f"default.{piscsi.common_settings.CONFIG_FILE_SUFFIX}"
# File containing canonical drive properties
DRIVE_PROPERTIES_FILE = WEB_DIR + "/drive_properties.json"

# The user group that is used for webapp authentication
AUTH_GROUP = "piscsi"

# The language locales supported by PiSCSI
LANGUAGES = ["en", "de", "sv", "fr", "es", "zh"]

# Available themes
TEMPLATE_THEMES = ["classic", "modern"]

# Default theme for modern browsers
TEMPLATE_THEME_DEFAULT = "modern"

# Fallback theme for older browsers
TEMPLATE_THEME_LEGACY = "classic"
