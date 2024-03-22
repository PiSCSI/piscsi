"""Module for creating a menu"""

from typing import List


class Menu:
    """Class implement the Menu class"""

    def __init__(self, name: str):
        self.entries: List = []
        self.item_selection = 0
        self.name = name
        self.context_object = None

    def add_entry(self, text, data_object=None):
        """Adds an entry to a menu"""
        entry = {"text": text, "data_object": data_object}
        self.entries.append(entry)

    def get_current_text(self):
        """Returns the text content of the currently selected text in the menu."""
        return self.entries[self.item_selection]["text"]

    def get_current_info_object(self):
        """Returns the data object to the currently selected menu item"""
        return self.entries[self.item_selection]["data_object"]

    def __repr__(self):
        print("entries: " + str(self.entries))
        print("item_selection: " + str(self.item_selection))
        print("name: " + self.name)
        print("context object: " + str(self.context_object))
