"""
Module for central PiSCSI control board configuration parameters
"""

from ctrlboard_hw.ctrlboard_hw_constants import CtrlBoardHardwareConstants


# pylint: disable=too-few-public-methods
class CtrlboardConfig:
    """Class for central PiSCSI control board configuration parameters"""

    ROTATION = 0
    WIDTH = 128
    HEIGHT = 64
    LINES = 8
    TOKEN = ""
    BORDER = 5
    TRANSITIONS = 1
    DISPLAY_I2C_ADDRESS = CtrlBoardHardwareConstants.DISPLAY_I2C_ADDRESS
    PCA9554_I2C_ADDRESS = CtrlBoardHardwareConstants.PCA9554_I2C_ADDRESS
    MENU_REFRESH_INTERVAL = 6
    LOG_LEVEL = 30  # Warning

    PISCSI_HOST = "localhost"
    PISCSI_PORT = "6868"

    def __str__(self):
        result = "rotation: " + str(self.ROTATION) + "\n"
        result += "width: " + str(self.WIDTH) + "\n"
        result += "height: " + str(self.HEIGHT) + "\n"
        result += "lines: " + str(self.LINES) + "\n"
        result += "border: " + str(self.BORDER) + "\n"
        result += "piscsi host: " + str(self.PISCSI_HOST) + "\n"
        result += "piscsi port: " + str(self.PISCSI_PORT) + "\n"
        result += "transitions: " + str(self.TRANSITIONS) + "\n"

        return result
