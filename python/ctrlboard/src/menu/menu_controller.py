"""Module providing the menu controller."""

import time
import importlib
from typing import Dict, Optional
from PIL import Image

from menu.menu import Menu
from menu.menu_builder import MenuBuilder
from menu.menu_renderer_config import MenuRendererConfig
from menu.menu_renderer_luma_oled import MenuRendererLumaOled
from menu.transition import Transition


class MenuController:
    """Class providing the menu controller. The menu controller is a central class
    that controls the menu and its associated rendering to a screen."""

    def __init__(self, menu_builder: MenuBuilder, menu_renderer=None, menu_renderer_config=None):
        self._menus: Dict[str, Menu] = {}
        self._active_menu: Optional[Menu] = None
        self._menu_renderer = menu_renderer
        self._menu_builder: MenuBuilder = menu_builder
        self._menu_renderer_config: Optional[MenuRendererConfig]
        if menu_renderer_config is None:
            self._menu_renderer_config = MenuRendererConfig()
        else:
            self._menu_renderer_config = menu_renderer_config

        if menu_renderer is None:  # default to LumaOled renderer if nothing else is stated
            self._menu_renderer = MenuRendererLumaOled(self._menu_renderer_config)
        else:
            self._menu_renderer = menu_renderer
        self._transition: Optional[Transition] = None
        if self._menu_renderer_config.transition is None:
            self._transition = None
            return
        try:
            module = importlib.import_module("menu.transition")
            try:
                transition_class = getattr(module, self._menu_renderer_config.transition)
                if transition_class is not None:
                    self._transition = transition_class(self._menu_renderer.disp)
            except AttributeError:
                pass
        except ImportError:
            print("transition module does not exist. Falling back to default.")
            self._transition = None

    def add(self, name: str, context_object=None):
        """Adds a menu to the menu collection internal to the controller by name.
        The associated class menu builder builds the menu by name."""
        self._menus[name] = self._menu_builder.build(name)
        if context_object is not None:
            self._menus[name].context_object = context_object

    def set_active_menu(self, name: str, display_on_device=True):
        """Activates a menu from the controller internal menu collection by name."""
        self._active_menu = self._menus[name]
        self._menu_renderer.set_menu(self._active_menu)
        self._menu_renderer.render(display_on_device)

    def refresh(self, name: str, context_object=None):
        """Refreshes a menu by name (by calling the menu builder again to rebuild the menu)."""
        item_selection = None
        if self._menus.get(name) is not None:
            item_selection = self._menus[name].item_selection
        self._menus[name] = self._menu_builder.build(name, context_object)

        if context_object is not None:
            self._menus[name].context_object = context_object

        if item_selection is not None:
            self._menus[name].item_selection = item_selection

    def get_menu(self, name: str):
        """Returns the controller internal menu collection"""
        return self._menus[name]

    def get_active_menu(self):
        """Returns the currently activated menu"""
        return self._active_menu

    def get_menu_renderer_config(self):
        """Returns the menu renderer configuration"""
        return self._menu_renderer_config

    def get_menu_renderer(self):
        """Returns the menu renderer for this menu controller"""
        return self._menu_renderer

    def segue(self, name, context_object=None, transition_attributes=None):
        """Transitions one menu into the other with all associated actions such
        as transition animations."""
        self.get_active_menu().context_object = None
        self.refresh(name, context_object)

        if self._transition is not None and transition_attributes is not None:
            source_image = self._menu_renderer.image.copy()
            transition_menu = self.get_menu(name)
            self._menu_renderer.set_menu(transition_menu)
            target_image = self._menu_renderer.render(display_on_device=False)
            transition_attributes["transition_speed"] = self._menu_renderer_config.transition_speed
            self._transition.perform(source_image, target_image, transition_attributes)

        self.set_active_menu(name)

    def show_message(self, message: str, sleep=1):
        """Displays a blocking message on the screen that stays for sleep seconds"""
        self.get_menu_renderer().message = message
        self.get_menu_renderer().render()
        time.sleep(sleep)
        self.get_menu_renderer().message = ""

    def show_timed_message(self, message: str):
        """Shows a message on the screen. The timed message is non-blocking for the main loop and
        simply redraws the message on screen if necessary."""
        self.get_menu_renderer().message = message
        self.get_menu_renderer().render()

    def show_mini_message(self, message: str, sleep=1):
        """The mini message is a message on the screen that only coveres the center portion
        of the screen, i.e., the remaining content is still visible on the screen while the mini
        message is shown in the middle. This version is blocking and stays for sleep seconds."""
        self.get_menu_renderer().mini_message = message
        self.get_menu_renderer().render()
        time.sleep(sleep)
        self.get_menu_renderer().mini_message = ""

    def show_timed_mini_message(self, message: str):
        """The mini message is a message on the screen that only coveres the center portion of
        the screen, i.e., the remaining content is still visible on the screen while the mini
        message is shown in the middle. This version is non-blocking for the main loop and
        simply redraws the mini message on screen if necessary."""
        self.get_menu_renderer().mini_message = message
        self.get_menu_renderer().render()

    def show_splash_screen(self, filename, sleep=2):
        """Shows a splash screen for a given number of seconds."""
        image = Image.open(filename).convert("1")
        self.get_menu_renderer().update_display_image(image)
        time.sleep(sleep)

    def update(self):
        """Updates the menu / draws the screen if necessary."""
        self._menu_renderer.update()
