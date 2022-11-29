"""Module containing the RaSCSI Control Board hardware constants"""


# pylint: disable=too-few-public-methods
class CtrlBoardHardwareConstants:
    """Class containing the RaSCSI Control Board hardware constants"""

    DISPLAY_I2C_ADDRESS = 0x3C
    PCA9554_I2C_ADDRESS = 0x3F
    PCA9554_PIN_ENC_A = 0
    PCA9554_PIN_ENC_B = 1
    PCA9554_PIN_BUTTON_1 = 2
    PCA9554_PIN_BUTTON_2 = 3
    PCA9554_PIN_BUTTON_ROTARY = 5
    PCA9554_PIN_LED_1 = 6
    PCA9554_PIN_LED_2 = 7

    PI_PIN_INTERRUPT = 9  # BCM

    BUTTON_1 = "Bt1"
    BUTTON_2 = "Bt2"
    ROTARY_A = "RotA"
    ROTARY_B = "RotB"
    ROTARY_BUTTON = "RotBtn"
    ROTARY = "Rot"
