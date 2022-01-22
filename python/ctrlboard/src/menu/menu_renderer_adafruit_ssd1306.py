from board import SCL, SDA
import busio
import adafruit_ssd1306
from menu.menu_renderer import MenuRenderer


class MenuRendererAdafruitSSD1306(MenuRenderer):

    def display_init(self):
        i2c = busio.I2C(SCL, SDA)
        self.disp = adafruit_ssd1306.SSD1306_I2C(self._config.width, self._config.height, i2c,
                                                 addr=self._config.ssd1306_i2c_address)
        self.disp.rotation = self._config.get_mapped_rotation()
        self.disp.fill(0)
        self.disp.show()

        return self.disp

    def update_display_image(self, image):
        self.disp.image(self.image)
        self.disp.show()

    def update_display(self):
        self.disp.show()

    def display_clear(self):
        self.disp.fill(0)
