"""Module containing an abstraction for the hardware button through the i2c multiplexer"""


# pylint: disable=too-few-public-methods
class HardwareButton:
    """Class implementing a hardware button interface that uses the i2c multiplexer"""

    def __init__(self, pca_driver, pin):
        self.pca_driver = pca_driver
        self.pin = pin
        self.state = True
        self.state_interrupt = True
        self.name = "n/a"
        self.last_press = None

    def read(self):
        """Reads the configured port of the i2c multiplexer"""
        return self.pca_driver.read_input_register_port(self.pin)
