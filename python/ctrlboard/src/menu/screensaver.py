import time
from abc import abstractmethod


class ScreenSaver:

    def __init__(self, activation_delay, menu_renderer):
        self.enabled = False
        self.menu_renderer = menu_renderer
        self.screensaver_activation_delay = activation_delay
        self.start_timestamp = int(time.time())

    def draw(self):
        if self.enabled is True:
            self.draw_screensaver()

    @abstractmethod
    def draw_screensaver(self):
        pass

    def check_timer(self):
        current_timestamp = int(time.time())
        timestamp_diff = current_timestamp - self.start_timestamp

        if timestamp_diff >= self.screensaver_activation_delay:
            self.enabled = True

    def reset_timer(self):
        self.start_timestamp = int(time.time())
        self.enabled = False
