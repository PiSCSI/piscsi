"""Module providing the interface to the PiSCSI Control Board hardware"""

# noinspection PyUnresolvedReferences
import logging
import time

import RPi.GPIO as GPIO
import smbus
from ctrlboard_hw import pca9554multiplexer
from ctrlboard_hw.hardware_button import HardwareButton
from ctrlboard_hw.ctrlboard_hw_constants import CtrlBoardHardwareConstants
from ctrlboard_hw.encoder import Encoder
from ctrlboard_hw.pca9554multiplexer import PCA9554Multiplexer
from observable import Observable


# pylint: disable=too-many-instance-attributes
class CtrlBoardHardware(Observable):
    """Class implements the PiSCSI Control Board hardware and provides an interface to it."""

    def __init__(self, display_i2c_address, pca9554_i2c_address, debounce_ms=200):
        self.display_i2c_address = display_i2c_address
        self.pca9554_i2c_address = pca9554_i2c_address
        self.debounce_ms = debounce_ms
        self.piscsi_controlboard_detected = self.detect_piscsi_controlboard()
        log = logging.getLogger(__name__)
        log.info("PiSCSI Control Board detected: %s", str(self.piscsi_controlboard_detected))
        self.display_detected = self.detect_display()
        log.info("Display detected: %s", str(self.display_detected))

        if self.piscsi_controlboard_detected is False:
            return

        self.pos = 0
        self.pca_driver = pca9554multiplexer.PCA9554Multiplexer(self.pca9554_i2c_address)

        # setup pca9554
        self.pca_driver.write_configuration_register_port(
            CtrlBoardHardwareConstants.PCA9554_PIN_ENC_A,
            PCA9554Multiplexer.PIN_ENABLED_AS_INPUT,
        )
        self.pca_driver.write_configuration_register_port(
            CtrlBoardHardwareConstants.PCA9554_PIN_ENC_B,
            PCA9554Multiplexer.PIN_ENABLED_AS_INPUT,
        )
        self.pca_driver.write_configuration_register_port(
            CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_1,
            PCA9554Multiplexer.PIN_ENABLED_AS_INPUT,
        )
        self.pca_driver.write_configuration_register_port(
            CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_2,
            PCA9554Multiplexer.PIN_ENABLED_AS_INPUT,
        )
        self.pca_driver.write_configuration_register_port(
            CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_ROTARY,
            PCA9554Multiplexer.PIN_ENABLED_AS_INPUT,
        )
        self.pca_driver.write_configuration_register_port(
            CtrlBoardHardwareConstants.PCA9554_PIN_LED_1,
            PCA9554Multiplexer.PIN_ENABLED_AS_OUTPUT,
        )
        self.pca_driver.write_configuration_register_port(
            CtrlBoardHardwareConstants.PCA9554_PIN_LED_2,
            PCA9554Multiplexer.PIN_ENABLED_AS_OUTPUT,
        )
        self.input_register_buffer = 0

        # pylint: disable=no-member
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(CtrlBoardHardwareConstants.PI_PIN_INTERRUPT, GPIO.IN)
        GPIO.add_event_detect(
            CtrlBoardHardwareConstants.PI_PIN_INTERRUPT,
            GPIO.FALLING,
            callback=self.button_pressed_callback,
        )

        # configure button of the rotary encoder
        self.rotary_button = HardwareButton(
            self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_ROTARY
        )
        self.rotary_button.state = True
        self.rotary_button.name = CtrlBoardHardwareConstants.ROTARY_BUTTON

        # configure button 1
        self.button1 = HardwareButton(
            self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_1
        )
        self.button1.state = True
        self.button1.name = CtrlBoardHardwareConstants.BUTTON_1

        # configure button 2
        self.button2 = HardwareButton(
            self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_2
        )
        self.button2.state = True
        self.button2.name = CtrlBoardHardwareConstants.BUTTON_2

        # configure rotary encoder pin a
        self.rotary_a = HardwareButton(
            self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_ENC_A
        )
        self.rotary_a.state = True
        self.rotary_a.directionalTransition = False
        self.rotary_a.name = CtrlBoardHardwareConstants.ROTARY_A

        # configure rotary encoder pin b
        self.rotary_b = HardwareButton(
            self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_ENC_B
        )
        self.rotary_b.state = True
        self.rotary_b.directionalTransition = False
        self.rotary_b.name = CtrlBoardHardwareConstants.ROTARY_B

        # configure encoder object
        self.rotary = Encoder(self.rotary_a, self.rotary_b)
        self.rotary.pos_prev = 0
        self.rotary.name = CtrlBoardHardwareConstants.ROTARY
        self.rot_prev_state = 0b11

    # noinspection PyUnusedLocal
    # pylint: disable=unused-argument
    def button_pressed_callback(self, channel):
        """Method is called when a button is pressed and reads the corresponding input register."""
        input_register = self.pca_driver.read_input_register()
        self.input_register_buffer <<= 8
        self.input_register_buffer |= input_register

    def check_button_press(self, button):
        """Checks whether the button state has changed."""
        if button.state_interrupt is True:
            return

        value = button.state_interrupt

        # ignore button press if debounce time is not reached
        if button.last_press is not None:
            elapsed = (time.time_ns() - button.last_press) / 1000000
            if elapsed < self.debounce_ms:
                return

        if value != button.state and value is False:
            button.state = False
            self.notify(button)
            button.state = True
            button.state_interrupt = True
            button.last_press = time.time_ns()

    def check_rotary_encoder(self, rotary):
        """Checks whether the rotary state has changed."""
        rotary.update()
        if self.rotary.pos_prev != self.rotary.pos:
            self.notify(rotary)
            self.rotary.pos_prev = self.rotary.pos

    # noinspection PyMethodMayBeStatic
    @staticmethod
    def button_value(input_register_buffer, bit):
        """Method reads the button value from a specific bit in the input register."""
        tmp = input_register_buffer
        bitmask = 1 << bit
        tmp &= bitmask
        tmp >>= bit
        return tmp

    # noinspection PyMethodMayBeStatic
    @staticmethod
    def button_value_shifted_list(input_register_buffer, bit):
        """Helper method for dealing with multiple buffered input registers"""
        input_register_buffer_length = int(len(format(input_register_buffer, "b")) / 8)
        shiftval = (input_register_buffer_length - 1) * 8
        tmp = input_register_buffer >> shiftval
        bitmask = 1 << bit
        tmp &= bitmask
        tmp >>= bit
        return tmp

    def process_events(self):
        """Non-blocking event processor for hardware events (button presses etc.)"""
        input_register_buffer = self.input_register_buffer
        self.input_register_buffer = 0

        input_register_buffer_length = int(len(format(input_register_buffer, "b")) / 8)
        if input_register_buffer_length < 1:
            return

        for i in range(0, input_register_buffer_length):
            shiftval = (input_register_buffer_length - 1 - i) * 8
            input_register = (input_register_buffer >> shiftval) & 0b11111111

            rot_a = self.button_value(input_register, 0)
            rot_b = self.button_value(input_register, 1)

            button_rotary = self.button_value(input_register, 5)
            button_1 = self.button_value(input_register, 2)
            button_2 = self.button_value(input_register, 3)

            if button_1 == 0:
                self.button1.state_interrupt = bool(button_1)

            if button_2 == 0:
                self.button2.state_interrupt = bool(button_2)

            if button_rotary == 0:
                self.rotary_button.state_interrupt = bool(button_rotary)

            rot_tmp_state = 0b11
            if rot_a == 0:
                rot_tmp_state &= 0b10
            if rot_b == 0:
                rot_tmp_state &= 0b01

            if rot_tmp_state != self.rot_prev_state:
                self.rot_prev_state = rot_tmp_state
                self.rotary.enc_a.state_interrupt = bool(rot_a)
                self.rotary.enc_b.state_interrupt = bool(rot_b)
                self.check_rotary_encoder(self.rotary)

            self.check_button_press(self.rotary_button)
            self.check_button_press(self.button1)
            self.check_button_press(self.button2)

    @staticmethod
    def detect_i2c_devices(_bus):
        """Method finds addresses on the i2c bus"""
        detected_i2c_addresses = []

        for _address in range(128):
            if 2 < _address < 120:
                try:
                    _bus.read_byte(_address)
                    address = "%02x" % _address
                    detected_i2c_addresses.append(int(address, base=16))
                except IOError:  # simply skip unsuccessful i2c probes
                    pass

        return detected_i2c_addresses

    def detect_piscsi_controlboard(self):
        """Detects whether the PiSCSI Control Board is attached by checking whether
        the expected i2c addresses are detected."""
        # pylint: disable=c-extension-no-member
        i2c_addresses = self.detect_i2c_devices(smbus.SMBus(1))
        return bool(
            (
                int(self.display_i2c_address) in i2c_addresses
                and (int(self.pca9554_i2c_address) in i2c_addresses)
            )
        )

    def detect_display(self):
        """Detects whether an i2c display is connected to the PiSCSI hat."""
        # pylint: disable=c-extension-no-member
        i2c_addresses = self.detect_i2c_devices(smbus.SMBus(1))
        return bool(int(self.display_i2c_address) in i2c_addresses)

    @staticmethod
    def cleanup():
        """Resets pin and interrupt settings on the pins."""
        # pylint: disable=no-member
        GPIO.cleanup()
