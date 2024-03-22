"""Module provides the abstract menu renderer class"""

import time
import math
import itertools

from abc import ABC, abstractmethod
from pydoc import locate

from typing import Optional
from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont
from menu.menu import Menu
from menu.menu_renderer_config import MenuRendererConfig
from menu.screensaver import ScreenSaver


class MenuRenderer(ABC):
    """The abstract menu renderer class provides the base for concrete menu
    renderer classes that implement functionality based on conrete hardware or available APIs."""

    def __init__(self, config: MenuRendererConfig):
        self.message = ""
        self.mini_message = ""
        self._menu = None
        self._config = config
        self.disp = self.display_init()

        self.image = Image.new("1", (self.disp.width, self.disp.height))
        self.draw = ImageDraw.Draw(self.image)
        self.font = ImageFont.truetype(config.font_path, size=config.font_size)
        # just a sample text to work with the font height
        self.font_height = self.font.getbbox("ABCabc")[3]
        self.cursor_position = 0
        self.frame_start_row = 0
        self.render_timestamp = None
        # effectively a small state machine that deals with the scrolling
        self._perform_scrolling_stage = 0
        self._x_scrolling = 0
        self._current_line_horizontal_overlap = None
        self._stage_timestamp: Optional[int] = None

        screensaver = locate(self._config.screensaver)
        # noinspection PyCallingNonCallable
        self.screensaver: ScreenSaver = screensaver(self._config.screensaver_delay, self)

    @abstractmethod
    def display_init(self):
        """Method initializes the displays for usage."""

    @abstractmethod
    def display_clear(self):
        """Methods clears the screen. Possible hardware clear call if necessary."""

    @abstractmethod
    def blank_screen(self):
        """Method blanks the screen. Based on drawing a blank rectangle."""

    @abstractmethod
    def update_display_image(self, image):
        """Method displays an image using PIL."""

    @abstractmethod
    def update_display(self):
        """Method updates the display."""

    def set_config(self, config: MenuRendererConfig):
        """Configures the menu renderer with a generic menu renderer configuration."""
        self._config = config

    def get_config(self):
        """Returns the menu renderer configuration."""
        return self._config

    def set_menu(self, menu: Menu):
        """Method sets the menu that the menu renderer should draw."""
        self._menu = menu

    def rows_per_screen(self):
        """Calculates the number of rows per screen based on the configured font size."""
        rows = self.disp.height / self.font_height
        return math.floor(rows)

    def draw_row(self, row_number: int, text: str, selected: bool):
        """Draws a single row of the menu."""
        x_pos = 0
        y_pos = row_number * self.font_height
        if selected:
            selection_extension = 0
            if row_number < self.rows_per_screen():
                selection_extension = self._config.row_selection_pixel_extension
            self.draw.rectangle(
                (
                    x_pos,
                    y_pos,
                    self.disp.width,
                    y_pos + self._config.font_size + selection_extension,
                ),
                outline=0,
                fill=255,
            )

            # in stage 1, we initialize scrolling for the currently selected line
            if self._perform_scrolling_stage == 1:
                self.setup_horizontal_scrolling(text)
                self._perform_scrolling_stage = 2  # don't repeat once the scrolling has started

            # in stage 2, we know the details and can thus perform the scrolling to the left
            if self._perform_scrolling_stage == 2:
                if self._current_line_horizontal_overlap + self._x_scrolling > 0:
                    self._x_scrolling -= 1
                if self._current_line_horizontal_overlap + self._x_scrolling == 0:
                    self._stage_timestamp = int(time.time())
                    self._perform_scrolling_stage = 3

            # in stage 3, we wait for a little when we have scrolled to the end of the text
            if self._perform_scrolling_stage == 3:
                current_time = int(time.time())
                time_diff = current_time - self._stage_timestamp
                if time_diff >= int(self._config.scroll_line_end_delay):
                    self._stage_timestamp = None
                    self._perform_scrolling_stage = 4

            # in stage 4, we scroll back to the right
            if self._perform_scrolling_stage == 4:
                if self._current_line_horizontal_overlap + self._x_scrolling >= 0:
                    self._x_scrolling += 1

                if (
                    self._current_line_horizontal_overlap + self._x_scrolling
                ) == self._current_line_horizontal_overlap:
                    self._stage_timestamp = int(time.time())
                    self._perform_scrolling_stage = 5

            # in stage 5, we wait again for a little while before we start again
            if self._perform_scrolling_stage == 5:
                current_time = int(time.time())
                time_diff = current_time - self._stage_timestamp
                if time_diff >= int(self._config.scroll_line_end_delay):
                    self._stage_timestamp = None
                    self._perform_scrolling_stage = 2

            self.draw.text(
                (x_pos + self._x_scrolling, y_pos),
                text,
                font=self.font,
                spacing=0,
                stroke_fill=0,
                fill=0,
            )
        else:
            self.draw.text((x_pos, y_pos), text, font=self.font, spacing=0, stroke_fill=0, fill=255)

    def draw_fullsceen_message(self, text: str):
        """Draws a fullscreen message, i.e., a full-screen message."""
        font_width = self.font.getlength(text)
        font_height = self.font.getbbox(text)[3]
        centered_width = (self.disp.width - font_width) / 2
        centered_height = (self.disp.height - font_height) / 2

        self.draw.rectangle((0, 0, self.disp.width, self.disp.height), outline=0, fill=255)
        self.draw.text(
            (centered_width, centered_height),
            text,
            align="center",
            font=self.font,
            stroke_fill=0,
            fill=0,
            textsize=20,
        )

    def draw_mini_message(self, text: str):
        """Draws a fullscreen message, i.e., a message covering only the center portion of
        the screen. The remaining areas stay visible."""
        font_width = self.font.getlength(text)
        centered_width = (self.disp.width - font_width) / 2
        centered_height = (self.disp.height - self.font_height) / 2

        self.draw.rectangle(
            (
                0,
                centered_height - 4,
                self.disp.width,
                centered_height + self.font_height + 4,
            ),
            outline=0,
            fill=255,
        )
        self.draw.text(
            (centered_width, centered_height),
            text,
            align="center",
            font=self.font,
            stroke_fill=0,
            fill=0,
            textsize=20,
        )

    def draw_menu(self):
        """Method draws the menu set to the class instance."""
        if self._menu.item_selection >= self.frame_start_row + self.rows_per_screen():
            if self._config.scroll_behavior == "page":
                self.frame_start_row = (
                    self.frame_start_row + (round(self.rows_per_screen() / 2)) + 1
                )
                if self.frame_start_row > len(self._menu.entries) - self.rows_per_screen():
                    self.frame_start_row = len(self._menu.entries) - self.rows_per_screen()
            else:  # extend as default behavior
                self.frame_start_row = self._menu.item_selection + 1 - self.rows_per_screen()

        if self._menu.item_selection < self.frame_start_row:
            if self._config.scroll_behavior == "page":
                self.frame_start_row = (
                    self.frame_start_row - (round(self.rows_per_screen() / 2)) - 1
                )
                if self.frame_start_row < 0:
                    self.frame_start_row = 0
            else:  # extend as default behavior
                self.frame_start_row = self._menu.item_selection

        self.draw_menu_frame(self.frame_start_row, self.frame_start_row + self.rows_per_screen())

    def draw_menu_frame(self, frame_start_row: int, frame_end_row: int):
        """Draws row frame_start_row to frame_end_row of the class instance menu, i.e., it
        draws a given frame of the complete menu that fits the screen."""
        self.draw.rectangle((0, 0, self.disp.width, self.disp.height), outline=0, fill=0)
        row_on_screen = 0
        row_in_menuitems = frame_start_row
        for menu_entry in itertools.islice(self._menu.entries, frame_start_row, frame_end_row):
            if row_in_menuitems == self._menu.item_selection:
                self.draw_row(row_on_screen, menu_entry["text"], True)
            else:
                self.draw_row(row_on_screen, menu_entry["text"], False)
            row_in_menuitems += 1
            row_on_screen += 1
            if row_on_screen >= self.rows_per_screen():
                break

    def render(self, display_on_device=True):
        """Method renders the menu."""
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
            return None

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
        """Configure horizontal scrolling based on the configured screen dimensions."""
        font_width = self.font.getlength(text)
        self._current_line_horizontal_overlap = font_width - self.disp.width

    def update(self):
        """Method updates the menu drawing within the main loop in a non-blocking manner.
        Also updates the current entry scrolling if activated."""
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
