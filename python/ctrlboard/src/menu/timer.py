"""Module providing a timer class"""

import time


class Timer:
    """Class implementing a timer class. Takes an activation delay and
    sets a flag if the activation delay exprires."""

    def __init__(self, activation_delay):
        self.start_timestamp = int(time.time())
        self.activation_delay = activation_delay
        self.enabled = False

    def check_timer(self):
        """Checks the timer whether it has reached the activation delay."""
        current_timestamp = int(time.time())
        timestamp_diff = current_timestamp - self.start_timestamp

        if timestamp_diff >= self.activation_delay:
            self.enabled = True

    def reset_timer(self):
        """Resets the timer and starts from the beginning."""
        self.start_timestamp = int(time.time())
        self.enabled = False
