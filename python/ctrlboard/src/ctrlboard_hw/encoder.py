"""Module containing an implementation for reading the rotary encoder directions through
the i2c multiplexer + interrupt"""

from ctrlboard_hw.hardware_button import HardwareButton


class Encoder:
    """Class implementing a detection mechanism to detect the rotary encoder directions
    through the i2c multiplexer + interrupt"""

    def __init__(self, enc_a: HardwareButton, enc_b: HardwareButton):
        self.enc_a = enc_a
        self.enc_b = enc_b
        self.pos = 0
        self.state = 0b00000000
        self.direction = 0

    def update(self):
        """Updates the internal attributes wrt. to the encoder position and direction."""
        value_enc_a = self.enc_a.state_interrupt
        value_enc_b = self.enc_b.state_interrupt

        self.direction = 0
        state = self.state & 0b00111111

        if value_enc_a:
            state |= 0b01000000
        if value_enc_b:
            state |= 0b10000000

        # clockwise pattern detection
        if (
            state == 0b11010010
            or state == 0b11001000
            or state == 0b11011000
            or state == 0b11010001
            or state == 0b11011011
            or state == 0b11100000
            or state == 0b11001011
        ):
            self.pos += 1
            self.direction = 1
            self.state = 0b00000000
            return
        # counter-clockwise pattern detection
        elif (
            state == 0b11000100
            or state == 0b11100100
            or state == 0b11100001
            or state == 0b11000111
            or state == 0b11100111
        ):
            self.pos -= 1
            self.direction = -1
            self.state = 0b00000000
            return

        self.state = state >> 2
