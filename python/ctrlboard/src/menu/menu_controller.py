import time
from typing import Dict, Optional
from menu.menu import Menu
from menu.menu_builder import MenuBuilder
from menu.menu_renderer import MenuRenderer
from menu.menu_renderer_config import MenuRendererConfig
import importlib
from menu.transition import Transition


class MenuController:

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

        if menu_renderer is None:
            self._menu_renderer = MenuRenderer(self._menu_renderer_config)
        else:
            self._menu_renderer = menu_renderer
        self._transition: Optional[Transition] = None
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
        self._menus[name] = self._menu_builder.build(name)
        if context_object is not None:
            self._menus[name].context_object = context_object

    def set_active_menu(self, name: str, display_on_device=True):
        self._active_menu = self._menus[name]
        self._menu_renderer.set_menu(self._active_menu)
        self._menu_renderer.render(display_on_device)

    def refresh(self, name: str, context_object=None):
        item_selection = None
        if self._menus.get(name) is not None:
            item_selection = self._menus[name].item_selection
        self._menus[name] = self._menu_builder.build(name, context_object)

        if context_object is not None:
            self._menus[name].context_object = context_object

        if item_selection is not None:
            self._menus[name].item_selection = item_selection

    def get_menu(self, name: str):
        return self._menus[name]

    def get_active_menu(self):
        return self._active_menu

    def get_menu_renderer(self):
        return self._menu_renderer

    def get_rascsi_client(self):
        return self._menu_builder.get_rascsi_client()

    def segue(self, name, context_object=None, transition_attributes=None):
        self.get_active_menu().context_object = None
        self.refresh(name, context_object)

        if self._transition is not None:
            source_image = self._menu_renderer.image.copy()
            transition_menu = self.get_menu(name)
            self._menu_renderer.set_menu(transition_menu)
            target_image = self._menu_renderer.render(display_on_device=False)
            transition_attributes["transition_speed"] = self._menu_renderer_config.transition_speed
            self._transition.perform(source_image, target_image, transition_attributes)

        self.set_active_menu(name)

    def show_message(self, message: str, sleep=1):
        self.get_menu_renderer().message = message
        self.get_menu_renderer().render()
        time.sleep(1)
        self.get_menu_renderer().message = ""

    def update(self):
        self._menu_renderer.update()
