"""Module providing the menu screensaver class"""

from abc import abstractmethod
from menu.timer import Timer


class ScreenSaver:
    """Class implementing the menu screensaver"""

    def __init__(self, activation_delay, menu_renderer):
        self.enabled = False
        self.menu_renderer = menu_renderer
        self.screensaver_activation_delay = activation_delay
        self.timer_flag = Timer(self.screensaver_activation_delay)

    def draw(self):
        """Draws the screen saver in a non-blocking way if enabled."""
        if self.enabled is True:
            self.draw_screensaver()

    @abstractmethod
    def draw_screensaver(self):
        """Draws the screen saver. Must be implemented in subclasses."""

    def check_timer(self):
        """Checks if the screen saver should be enabled given the configured
        activation delay."""
        self.timer_flag.check_timer()
        self.enabled = self.timer_flag.enabled

    def reset_timer(self):
        """Resets the screen saver timer if an activitiy happend."""
        self.timer_flag.reset_timer()
        self.enabled = self.timer_flag.enabled
