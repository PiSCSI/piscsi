"""Module for configuring menu renderer instances"""


# pylint: disable=too-many-instance-attributes, too-few-public-methods
class MenuRendererConfig:
    """Class for configuring menu renderer instances. Provides configuration options
    such as width, height, i2c address, font, transitions, etc."""

    _rotation_mapper = {0: 0, 90: 1, 180: 2, 270: 3}

    def __init__(self):
        self.width = 128
        self.height = 64
        self.i2c_address = 0x3C
        self.i2c_port = 1
        self.display_type = "ssd1306"  # luma-oled supported devices, "sh1106", "ssd1306", ...
        self.font_path = "resources/DejaVuSansMono-Bold.ttf"
        self.font_size = 12
        self.row_selection_pixel_extension = 2
        self.scroll_behavior = "page"  # "extend" or "page"
        self.transition = "PushTransition"  # "PushTransition" or "None
        self.transition_attributes_left = {"direction": "push_left"}
        self.transition_attributes_right = {"direction": "push_right"}
        self.transition_speed = 10
        self.scroll_line = True
        self.scroll_delay = 3
        self.scroll_line_end_delay = 2
        self.screensaver = "menu.blank_screensaver.BlankScreenSaver"
        self.screensaver_delay = 25
        self.rotation = 0  # 0, 180

    def get_mapped_rotation(self):
        """Converts human-readable rotation value to the one expected
        by the luma and adafruit libraries"""
        return self._rotation_mapper[self.rotation]
