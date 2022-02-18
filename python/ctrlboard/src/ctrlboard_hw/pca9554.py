"""
Pca9554 driver, NXP Semiconductors
8 bit I2C expander, 8 GPIO ports
I2C SMBus protocol
Manual: PCA9554_9554A.pdf

Forked from public domain Pca9554 driver: https://github.com/IRNAS/pca9554-python
"""
import logging
import smbus

# selected i2c channel on rpi
I2C_CHANNEL = 1

# Define command byte registers
IN_REG = 0x00   # Input register
OUT_REG = 0x01  # Output register
POL_REG = 0x02  # Polarity inversion register
CONF_REG = 0x03  # Config register (0=output, 1=input)

OUTPUT = 0
INPUT = 1


# pylint: disable=bare-except
class Pca9554:
    """Class implementing a Pca9554 driver"""
    def __init__(self, address):
        """Init smbus channel and Pca9554 driver on specified address."""
        try:
            self.ports_count = 8    # number of GPIO ports

            self.i2c_bus = smbus.SMBus(I2C_CHANNEL)
            self.i2c_address = address
            if self.read_input_register() is None:
                raise ValueError
        except ValueError:
            logging.error("No device found on specified address!")
            self.i2c_bus = None
        except:
            logging.error("Bus on channel %s is not available.", format(I2C_CHANNEL))
            logging.info("Available busses are listed as /dev/i2c*")
            self.i2c_bus = None

    def read_input_register(self):
        """Read incoming logic levels of the ports (returns actual pin value)."""
        try:
            return self.i2c_bus.read_byte_data(self.i2c_address, IN_REG)
        except:
            return None

    def read_output_register(self):
        """Read outgoing logic levels of the ports defined as outputs (returns flip-flop
        controlling the output select)."""
        try:
            return self.i2c_bus.read_byte_data(self.i2c_address, OUT_REG)
        except:
            return None

    def read_polarity_inversion_register(self):
        """Read if the polarity of input register ports data is inverted (returns for each pin:
        1=inverted or 0=retained)."""
        try:
            return self.i2c_bus.read_byte_data(self.i2c_address, POL_REG)
        except:
            return None

    def read_config_register(self):
        """Read configuration of all ports (returns for each pin 1=input or 0=output)."""
        try:
            return self.i2c_bus.read_byte_data(self.i2c_address, CONF_REG)
        except:
            return None

    def read_port(self, port_num):
        """Read port bit value (high=1 or low=0)."""
        try:
            if port_num < 0 or port_num >= self.ports_count:
                return None
            line_value = self.i2c_bus.read_byte_data(self.i2c_address, IN_REG)
            ret = ((line_value >> port_num) & 1)
            return ret
        except:
            return None

    def write_port(self, port_num, state):
        """Write port bit specified in port_num to state value: 0=low, 1=high. Returns
        True if successful."""
        try:
            if port_num < 0 or port_num >= self.ports_count:
                return False
            if state < 0 or state > 1:
                return False
            current_value = self.i2c_bus.read_byte_data(self.i2c_address, OUT_REG)
            if state:
                new_value = current_value | 1 << port_num
            else:
                new_value = current_value & (255 - (1 << port_num))
            self.i2c_bus.write_byte_data(self.i2c_address, OUT_REG, new_value)
            return True
        except:
            return False

    def write_output_register(self, value):
        """Write all ports to states specified in byte value (each pin bit: 0=low, 1=high).
        Returns True if successful."""
        try:
            if value < 0x00 or value > 0xff:
                return False
            self.i2c_bus.write_byte_data(self.i2c_address, OUT_REG, value)
            return True
        except:
            return False

    def write_polarity_inversion_register(self, value):
        """Write polarity (1=inverted, 0=retained) for each input register port data (value
        has all ports polarities). Returns True if successful."""
        try:
            if value < 0x00 or value > 0xff:
                return False
            self.i2c_bus.write_byte_data(self.i2c_address, POL_REG, value)
            return True
        except:
            return False

    def write_config_register(self, value):
        """Write all ports to desired configuration (1=input or 0=output), parameter:
        value (type: byte). Returns True if successful."""
        try:
            if value < 0x00 or value > 0xff:
                return False
            self.i2c_bus.write_byte_data(self.i2c_address, CONF_REG, value)
            return True
        except:
            return False

    def write_config_port(self, port_num, state):
        """Write port configuration (specified by port_num) to desired state: 1=input or
        0=output. Returns True if successful."""
        try:
            if port_num < 0 or port_num >= self.ports_count:
                return False
            if state < 0 or state > 1:
                return False
            current_value = self.i2c_bus.read_byte_data(self.i2c_address, CONF_REG)
            if state:
                new_value = current_value | 1 << port_num
            else:
                new_value = current_value & (255 - (1 << port_num))
            self.i2c_bus.write_byte_data(self.i2c_address, CONF_REG, new_value)
            return True
        except:
            return False

    def __del__(self):
        """Driver destructor."""
        self.i2c_bus = None
