from ctrlboard_hw import pca9554
from ctrlboard_hw.hardware_button import HardwareButton
from ctrlboard_hw.ctrlboard_hw_constants import CtrlBoardHardwareConstants
from ctrlboard_hw.encoder import Encoder
from observable import Observable
# noinspection PyUnresolvedReferences
import RPi.GPIO as GPIO
import numpy
import smbus
import logging


class CtrlBoardHardware(Observable):

    def __init__(self, display_i2c_address, pca9554_i2c_address):
        self.display_i2c_address = display_i2c_address
        self.pca9554_i2c_address = pca9554_i2c_address
        self.rascsi_controlboard_detected = self.detect_rascsi_controlboard()
        log = logging.getLogger(__name__)
        log.info("RaSCSI Control Board detected: " + str(self.rascsi_controlboard_detected))
        self.display_detected = self.detect_display()
        log.info("Display detected: " + str(self.display_detected))

        if self.rascsi_controlboard_detected is False:
            return

        self.pos = 0
        self.pca_driver = pca9554.Pca9554(self.pca9554_i2c_address)

        # setup pca9554
        self.pca_driver.write_config_port(CtrlBoardHardwareConstants.PCA9554_PIN_ENC_A, pca9554.INPUT)
        self.pca_driver.write_config_port(CtrlBoardHardwareConstants.PCA9554_PIN_ENC_B, pca9554.INPUT)
        self.pca_driver.write_config_port(CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_1, pca9554.INPUT)
        self.pca_driver.write_config_port(CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_2, pca9554.INPUT)
        self.pca_driver.write_config_port(CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_ROTARY, pca9554.INPUT)
        self.pca_driver.write_config_port(CtrlBoardHardwareConstants.PCA9554_PIN_LED_1, pca9554.OUTPUT)
        self.pca_driver.write_config_port(CtrlBoardHardwareConstants.PCA9554_PIN_LED_2, pca9554.OUTPUT)
        self.input_register_buffer = numpy.uint32(0)

        GPIO.setmode(GPIO.BCM)
        # GPIO.setup(CtrlBoardHardwareConstants.PI_PIN_INTERRUPT, GPIO.IN, pull_up_down=GPIO.PUD_UP)
        GPIO.setup(CtrlBoardHardwareConstants.PI_PIN_INTERRUPT, GPIO.IN)  # stronger hardware pull-up
        GPIO.add_event_detect(CtrlBoardHardwareConstants.PI_PIN_INTERRUPT, GPIO.FALLING,
                              callback=self.button_pressed_callback)

        # # configure button of the rotary encoder
        self.rotary_button = HardwareButton(self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_ROTARY)
        self.rotary_button.state = True
        self.rotary_button.name = CtrlBoardHardwareConstants.ROTARY_BUTTON

        # configure button 1
        self.button1 = HardwareButton(self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_1)
        self.button1.state = True
        self.button1.name = CtrlBoardHardwareConstants.BUTTON_1

        # configure button 2
        self.button2 = HardwareButton(self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_BUTTON_2)
        self.button2.state = True
        self.button2.name = CtrlBoardHardwareConstants.BUTTON_2

        # configure rotary encoder pin a
        self.rotary_a = HardwareButton(self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_ENC_A)
        self.rotary_a.state = True
        self.rotary_a.directionalTransition = False
        self.rotary_a.name = CtrlBoardHardwareConstants.ROTARY_A

        # configure rotary encoder pin b
        self.rotary_b = HardwareButton(self.pca_driver, CtrlBoardHardwareConstants.PCA9554_PIN_ENC_B)
        self.rotary_b.state = True
        self.rotary_b.directionalTransition = False
        self.rotary_b.name = CtrlBoardHardwareConstants.ROTARY_B

        # configure encoder object
        self.rotary = Encoder(self.rotary_a, self.rotary_b)
        self.rotary.pos_prev = 0
        self.rotary.name = CtrlBoardHardwareConstants.ROTARY

    # noinspection PyUnusedLocal
    def button_pressed_callback(self, channel):
        # self.last_interrupt_time = time.perf_counter_ns()
        input_register = self.pca_driver.read_input_register()
        # print(format(input_register, 'b'))
        self.input_register_buffer <<= 8
        self.input_register_buffer |= input_register

    def check_button_press(self, button):
        if button.state_interrupt is True:
            return

        value = button.state_interrupt

        if value != button.state and value is False:
            button.state = False
            self.notify(button)
            # print("button press: " + str(button))
            button.state = True
            button.state_interrupt = True

    def check_rotary_encoder(self, rotary):
        rotary.update()
        if self.rotary.pos_prev != self.rotary.pos:
            self.notify(rotary)
            self.rotary.pos_prev = self.rotary.pos

    # noinspection PyMethodMayBeStatic
    def button_value(self, input_register_buffer, bit):
        tmp = input_register_buffer
        bitmask = 1 << bit
        tmp &= bitmask
        tmp >>= bit
        return tmp

    # noinspection PyMethodMayBeStatic
    def button_value_shifted_list(self, input_register_buffer, bit):
        input_register_buffer_length = int(len(format(input_register_buffer, 'b'))/8)
        shiftval = (input_register_buffer_length-1)*8
        tmp = input_register_buffer >> shiftval
        bitmask = 1 << bit
        tmp &= bitmask
        tmp >>= bit
        return tmp

    def process_events(self):
        input_register_buffer_length = int(len(format(self.input_register_buffer, 'b'))/8)
        if input_register_buffer_length <= 1:
            return

        input_register_buffer = self.input_register_buffer
        self.input_register_buffer = 0

        for i in range(0, input_register_buffer_length):
            shiftval = (input_register_buffer_length-1-i)*8
            input_register = (input_register_buffer >> shiftval) & 0b11111111

            rot_a = self.button_value(input_register, 0)
            rot_b = self.button_value(input_register, 1)
            button_rotary = self.button_value(input_register, 5)
            button_1 = self.button_value(input_register, 2)
            button_2 = self.button_value(input_register, 3)
            button_3 = self.button_value(input_register, 4)

            if button_1 == 0:
                self.button1.state_interrupt = bool(button_1)

            if button_2 == 0:
                self.button2.state_interrupt = bool(button_2)

            if button_rotary == 0:
                self.rotary_button.state_interrupt = bool(button_rotary)

            if rot_a == 0:
                self.rotary.enc_a.state_interrupt = bool(rot_a)

            if rot_b == 0:
                self.rotary.enc_b.state_interrupt = bool(rot_b)

            self.check_button_press(self.rotary_button)
            self.check_button_press(self.button1)
            self.check_button_press(self.button2)
            self.check_rotary_encoder(self.rotary)

        self.rotary.state = 0b11
        self.input_register_buffer = 0

    @staticmethod
    def detect_i2c_devices(_bus):
        detected_i2c_addresses = []

        for _address in range(128):
            if 2 < _address < 120:
                try:
                    _bus.read_byte(_address)
                    address = '%02x' % _address
                    detected_i2c_addresses.append(int(address, base=16))
                except IOError as ex:  # simply skip unsuccessful i2c probes
                    pass

        return detected_i2c_addresses

    def detect_rascsi_controlboard(self):
        i2c_addresses = self.detect_i2c_devices(smbus.SMBus(1))
        if int(self.display_i2c_address) in i2c_addresses and int(self.pca9554_i2c_address) in i2c_addresses:
            return True
        else:
            return False

    def detect_display(self):
        i2c_addresses = self.detect_i2c_devices(smbus.SMBus(1))
        if int(self.display_i2c_address) in i2c_addresses:
            return True
        else:
            return False

    @staticmethod
    def cleanup():
        GPIO.cleanup()
