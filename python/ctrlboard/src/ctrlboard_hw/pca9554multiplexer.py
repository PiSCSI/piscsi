"""
Module for interfacting with the pca9554 multiplexer
"""

# pylint: disable=c-extension-no-member
import logging
import smbus


class PCA9554Multiplexer:
    """Class interfacing with the pca9554 multiplexer"""

    PIN_ENABLED_AS_OUTPUT = 0
    PIN_ENABLED_AS_INPUT = 1

    def __init__(self, i2c_address):
        """Constructor for the pc9554 multiplexer interface class"""
        self.i2c_address = i2c_address

        try:
            self.i2c_bus = smbus.SMBus(1)
            if self.read_input_register() is None:
                logging.error(
                    "PCA9554 initialization test on specified i2c address %s failed",
                    self.i2c_address,
                )
                self.i2c_bus = None
        except IOError:
            logging.error("Could not open the i2c bus.")
            self.i2c_bus = None

    def write_configuration_register_port(self, port_bit, bit_value):
        """Reconfigures the configuration register. Updates the specified
        port bit with bit_value. Returns true if successful, false otherwise."""
        try:
            if (0 <= port_bit <= 8) and (0 <= bit_value <= 1):
                configuration_register = self.i2c_bus.read_byte_data(self.i2c_address, 3)
                if bit_value:
                    updated_configuration_register = configuration_register | (1 << port_bit)
                else:
                    updated_configuration_register = configuration_register & (
                        0xFF - (1 << port_bit)
                    )
                self.i2c_bus.write_byte_data(self.i2c_address, 3, updated_configuration_register)
                return True

            return False
        except IOError:
            return False

    def read_input_register(self):
        """Reads the complete 8 bit input port register from pca9554"""
        try:
            return self.i2c_bus.read_byte_data(self.i2c_address, 0)
        except IOError:
            return None

    def read_input_register_port(self, port_bit):
        """Reads the input port register and returns the logic level of one specific port in the
        argument"""
        try:
            if 0 <= port_bit <= 8:
                input_register = self.i2c_bus.read_byte_data(self.i2c_address, 0)
                return (input_register >> port_bit) & 1

            return None
        except IOError:
            return None
