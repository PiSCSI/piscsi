from typing import List


class Menu:

    def __init__(self, name: str):
        self.entries: List = []
        self.item_selection = 0
        self.name = name
        self.context_object = None

    def add_entry(self, text, data_object=None):
        entry = {"text": text, "data_object": data_object}
        self.entries.append(entry)

    def get_current_text(self):
        return self.entries[self.item_selection]['text']

    def get_current_info_object(self):
        return self.entries[self.item_selection]['data_object']

