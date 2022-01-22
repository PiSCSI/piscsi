from ctrlboard_hw.hardware_button import HardwareButton


class Encoder:
    def __init__(self, enc_a: HardwareButton, enc_b: HardwareButton):
        self.enc_a = enc_a
        self.enc_b = enc_b
        self.pos = 0
        self.state = 0b0011
        self.direction = 0

    def update(self):
        self.update2()
        # self.update1()

    def update2(self):
        value_enc_a = self.enc_a.state_interrupt
        value_enc_b = self.enc_b.state_interrupt

        self.direction = 0
        state = self.state & 0b0011

        if value_enc_a:
            state |= 0b0100
        if value_enc_b:
            state |= 0b1000

        if state == 0b1011:
            self.pos += 1
            self.direction = 1

        if state == 0b0111:
            self.pos -= 1
            self.direction = -1

        self.state = state >> 2

        self.enc_a.state_interrupt = True
        self.enc_b.state_interrupt = True

    def update1(self):
        if self.enc_a.state_interrupt is True and self.enc_b.state_interrupt is True:
            return

        if self.enc_a.state_interrupt is False and self.enc_b.state_interrupt is False:
            self.enc_a.state_interrupt = True
            self.enc_b.state_interrupt = True
            return

        self.direction = 0

        if self.enc_a.state_interrupt is False:
            self.pos += 1
            self.direction = 1
        elif self.enc_a.state_interrupt is True:
            self.pos -= 1
            self.direction = -1

        self.enc_a.state_interrupt = True
        self.enc_b.state_interrupt = True

