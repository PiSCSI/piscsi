import time


class TimerFlag:

    def __init__(self, activation_delay):
        self.start_timestamp = int(time.time())
        self.activation_delay = activation_delay
        self.enabled = False

    def check_timer(self):
        current_timestamp = int(time.time())
        timestamp_diff = current_timestamp - self.start_timestamp

        if timestamp_diff >= self.activation_delay:
            self.enabled = True

    def reset_timer(self):
        self.start_timestamp = int(time.time())
        self.enabled = False
