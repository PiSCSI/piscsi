"""Module for creating menus"""

from abc import ABC, abstractmethod
from menu.menu import Menu


# pylint: disable=too-few-public-methods
class MenuBuilder(ABC):
    """Base class for menu builders"""

    def __init__(self):
        pass

    @abstractmethod
    def build(self, name: str, context_object=None) -> Menu:
        """builds a menu and gives it a name and a context object"""
