"""Module providing the luma oled menu renderer class"""

from luma.core.interface.serial import i2c
from menu.menu_renderer import MenuRenderer


class MenuRendererLumaOled(MenuRenderer):
    """Class implementing the luma oled menu renderer"""

    def display_init(self):
        serial = i2c(port=self._config.i2c_port, address=self._config.i2c_address)
        import luma.oled.device

        device = getattr(luma.oled.device, self._config.display_type)

        self.disp = device(
            serial_interface=serial,
            width=self._config.width,
            height=self._config.height,
            rotate=self._config.get_mapped_rotation(),
        )

        self.disp.clear()
        self.disp.show()

        return self.disp

    def update_display_image(self, image):
        self.disp.display(image)

    def update_display(self):
        self.disp.display(self.image)

    def display_clear(self):
        pass

    def blank_screen(self):
        self.disp.clear()
        self.draw.rectangle((0, 0, self.disp.width, self.disp.height), outline=0, fill=0)
        self.disp.show()
