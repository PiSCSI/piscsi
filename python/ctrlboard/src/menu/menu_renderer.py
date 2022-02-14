import time
from typing import Optional
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont
from menu.menu import Menu
from menu.menu_renderer_config import MenuRendererConfig
import math
import itertools
from abc import ABC, abstractmethod
from pydoc import locate

from menu.screensaver import ScreenSaver


class MenuRenderer(ABC):

    def __init__(self, config: MenuRendererConfig):
        self.message = ""
        self.mini_message = ""
        self._menu = None
        self._config = config
        self.disp = self.display_init()

        self.image = Image.new('1', (self.disp.width, self.disp.height))
        self.draw = ImageDraw.Draw(self.image)
        self.font = ImageFont.truetype(config.font_path, size=config.font_size)
        font_width, self.font_height = self.font.getsize("ABCabc")  # just a sample text to work with the font height
        self.cursor_position = 0
        self.frame_start_row = 0
        self.render_timestamp = None
        self._perform_scrolling_stage = 0  # effectively a small state machine that deals with the scrolling
        self._x_scrolling = 0
        self._current_line_horizontal_overlap = None
        self._stage_timestamp: Optional[int] = None

        screensaver = locate(self._config.screensaver)
        # noinspection PyCallingNonCallable
        self.screensaver: ScreenSaver = screensaver(self._config.screensaver_delay, self)

    @abstractmethod
    def display_init(self):
        pass

    @abstractmethod
    def display_clear(self):
        pass

    @abstractmethod
    def blank_screen(self):
        pass

    @abstractmethod
    def update_display_image(self, image):
        pass

    @abstractmethod
    def update_display(self):
        pass

    def set_config(self, config: MenuRendererConfig):
        self._config = config

    def get_config(self):
        return self._config

    def set_menu(self, menu: Menu):
        self._menu = menu

    def rows_per_screen(self):
        rows = self.disp.height / self.font_height
        return math.floor(rows)

    def draw_row(self, row_number: int, text: str, selected: bool):
        x = 0
        y = row_number*self.font_height
        if selected:
            selection_extension = 0
            if row_number < self.rows_per_screen():
                selection_extension = self._config.row_selection_pixel_extension
            self.draw.rectangle((x, y, self.disp.width, y+self._config.font_size+selection_extension), outline=0,
                                fill=255)

            # in stage 1, we initialize scrolling for the currently selected line
            if self._perform_scrolling_stage == 1:
                # print("stage 1")
                self.setup_horizontal_scrolling(text)
                # print("overlap: " + str(self._current_line_horizontal_overlap))
                self._perform_scrolling_stage = 2  # don't repeat once the scrolling has started

            # in stage 2, we know the details and can thus perform the scrolling to the left
            if self._perform_scrolling_stage == 2:
                # print("stage 2")
                # print("sum: " + str(self._current_line_horizontal_overlap+self._x_scrolling))
                if self._current_line_horizontal_overlap+self._x_scrolling > 0:
                    self._x_scrolling -= 1
                if self._current_line_horizontal_overlap+self._x_scrolling == 0:
                    self._stage_timestamp = int(time.time())
                    self._perform_scrolling_stage = 3

            # in stage 3, we wait for a little when we have scrolled to the end of the text
            if self._perform_scrolling_stage == 3:
                # print("stage 3")
                current_time = int(time.time())
                time_diff = current_time - self._stage_timestamp
                if time_diff >= int(self._config.scroll_line_end_delay):
                    self._stage_timestamp = None
                    self._perform_scrolling_stage = 4

            # in stage 4, we scroll back to the right
            if self._perform_scrolling_stage == 4:
                # print("stage 4")
                # print("sum: " + str(self._current_line_horizontal_overlap+self._x_scrolling))

                if self._current_line_horizontal_overlap+self._x_scrolling >= 0:
                    self._x_scrolling += 1
                if self._current_line_horizontal_overlap+self._x_scrolling == self._current_line_horizontal_overlap:
                    self._stage_timestamp = int(time.time())
                    self._perform_scrolling_stage = 5

            # in stage 5, we wait again for a little while before we start again
            if self._perform_scrolling_stage == 5:
                # print("stage 5")
                current_time = int(time.time())
                time_diff = current_time - self._stage_timestamp
                if time_diff >= int(self._config.scroll_line_end_delay):
                    self._stage_timestamp = None
                    self._perform_scrolling_stage = 2

            self.draw.text((x+self._x_scrolling, y), text, font=self.font, spacing=0, stroke_fill=0, fill=0)
        else:
            self.draw.text((x, y), text, font=self.font, spacing=0, stroke_fill=0, fill=255)

    def draw_fullsceen_message(self, text: str):
        font_width, font_height = self.font.getsize(text)
        centered_width = (self.disp.width - font_width) / 2
        centered_height = (self.disp.height - font_height) / 2

        self.draw.rectangle((0, 0, self.disp.width, self.disp.height), outline=0, fill=255)
        self.draw.text((centered_width, centered_height), text, align="center", font=self.font,
                       stroke_fill=0, fill=0, textsize=20)

    def draw_mini_message(self, text: str):
        font_width, font_height = self.font.getsize(text)
        centered_width = (self.disp.width - font_width) / 2
        centered_height = (self.disp.height - self.font_height) / 2

        self.draw.rectangle((0, centered_height-4, self.disp.width, centered_height+self.font_height+4),
                            outline=0, fill=255)
        self.draw.text((centered_width, centered_height), text, align="center", font=self.font,
                       stroke_fill=0, fill=0, textsize=20)

    def draw_menu(self):
        if self._menu.item_selection >= self.frame_start_row + self.rows_per_screen():
            if self._config.scroll_behavior == "page":
                self.frame_start_row = self.frame_start_row + (round(self.rows_per_screen()/2)) + 1
                if self.frame_start_row > len(self._menu.entries) - self.rows_per_screen():
                    self.frame_start_row = len(self._menu.entries) - self.rows_per_screen()
            else:  # extend as default behavior
                self.frame_start_row = self._menu.item_selection + 1 - self.rows_per_screen()

        if self._menu.item_selection < self.frame_start_row:
            if self._config.scroll_behavior == "page":
                self.frame_start_row = self.frame_start_row - (round(self.rows_per_screen()/2)) - 1
                if self.frame_start_row < 0:
                    self.frame_start_row = 0
            else:  # extend as default behavior
                self.frame_start_row = self._menu.item_selection

        # print("frame start: " + str(self.frame_start_row))
        # print("frame end: " + str(self.frame_start_row+self.rows_per_screen()))
        # print("position in menu: " + str(self.menu.item_selection))

        self.draw_menu_frame(self.frame_start_row, self.frame_start_row+self.rows_per_screen())

    def draw_menu_frame(self, frame_start_row: int, frame_end_row: int):
        self.draw.rectangle((0, 0, self.disp.width, self.disp.height), outline=0, fill=0)
        row_on_screen = 0
        row_in_menuitems = frame_start_row
        for menuEntry in itertools.islice(self._menu.entries, frame_start_row, frame_end_row):
            if row_in_menuitems == self._menu.item_selection:
                self.draw_row(row_on_screen, menuEntry["text"], True)
            else:
                self.draw_row(row_on_screen, menuEntry["text"], False)
            row_in_menuitems += 1
            row_on_screen += 1
            if row_on_screen >= self.rows_per_screen():
                break

    def render(self, display_on_device=True):
        if display_on_device is True:
            self.screensaver.reset_timer()
        self._perform_scrolling_stage = 0
        self._current_line_horizontal_overlap = None
        self._x_scrolling = 0

        if self._menu is None:
            self.draw_fullsceen_message("No menu set!")
            self.disp.image(self.image)

            if display_on_device is True:
                self.disp.show()
            return

        self.display_clear()

        if self.message != "":
            self.draw_fullsceen_message(self.message)
        elif self.mini_message != "":
            self.draw_mini_message(self.mini_message)
        else:
            self.draw_menu()

        if display_on_device is True:
            self.update_display_image(self.image)
            self.update_display()

        self.render_timestamp = int(time.time())

        return self.image

    def setup_horizontal_scrolling(self, text):
        font_width, font_height = self.font.getsize(text)
        self._current_line_horizontal_overlap = font_width - self.disp.width

    def update(self):
        if self._config.scroll_line is False:
            return

        self.screensaver.check_timer()

        if self.screensaver.enabled is True:
            self.screensaver.draw()
            return

        current_time = int(time.time())
        time_diff = current_time - self.render_timestamp

        if time_diff >= self._config.scroll_delay:
            if self._perform_scrolling_stage == 0:
                self._perform_scrolling_stage = 1
            self.draw_menu()
            self.update_display_image(self.image)



