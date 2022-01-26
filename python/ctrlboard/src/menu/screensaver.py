from abc import abstractmethod
from menu.timer_flag import TimerFlag


class ScreenSaver:

    def __init__(self, activation_delay, menu_renderer):
        self.enabled = False
        self.menu_renderer = menu_renderer
        self.screensaver_activation_delay = activation_delay
        self.timer_flag = TimerFlag(self.screensaver_activation_delay)

    def draw(self):
        if self.enabled is True:
            self.draw_screensaver()

    @abstractmethod
    def draw_screensaver(self):
        pass

    def check_timer(self):
        self.timer_flag.check_timer()
        self.enabled = self.timer_flag.enabled

    def reset_timer(self):
        self.timer_flag.reset_timer()
        self.enabled = self.timer_flag.enabled
