class MenuRendererConfig:

    _rotation_mapper = {
        0: 0,
        90: 1,
        180: 2,
        270: 3
    }

    def __init__(self):
        self.width = 128
        self.height = 64
        self.i2c_address = 0x3c
        self.i2c_port = 1
#        self.display_type = "sh1106"
        self.display_type = "ssd1306"
        #    self.font_path = 'resources/SourceCodePro-Bold.ttf'
        self.font_path = 'resources/DejaVuSansMono-Bold.ttf'
        self.font_size = 12
        self.row_selection_pixel_extension = 2
        self.scroll_behavior = "page"  # "extend" or "page"
        #    self.scroll_behavior = "extend"  # "extend" or "page"
        self.transition = "PushTransition"
#        self.transition = "None"
        self.transition_attributes_left = {"direction": "push_left"}
        self.transition_attributes_right = {"direction": "push_right"}
        self.transition_speed = 10
        self.scroll_line = True
        self.scroll_delay = 3
        self.scroll_line_end_delay = 2
        self.screensaver = "menu.blank_screensaver.BlankScreenSaver"
        self.screensaver_delay = 25
        self.rotation = 0  # degrees. Options: 0, 180

    def get_mapped_rotation(self):
        return self._rotation_mapper[self.rotation]


