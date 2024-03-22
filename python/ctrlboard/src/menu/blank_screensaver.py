"""Module implementing a blank screensaver"""

from menu.screensaver import ScreenSaver


class BlankScreenSaver(ScreenSaver):
    """Class implementing a blank screen safer that simply blanks the screen after a
    configured activation delay"""

    def __init__(self, activation_delay, menu_renderer):
        super().__init__(activation_delay, menu_renderer)
        self._initial_draw_call = None

    def draw_screensaver(self):
        if self._initial_draw_call is True:
            self.menu_renderer.blank_screen()
        else:
            self._initial_draw_call = False

    def check_timer(self):
        already_enabled = False
        if self.enabled is True:
            already_enabled = True

        super().check_timer()

        if self.enabled is True and already_enabled is False:  # new switch to screensaver
            self._initial_draw_call = True
