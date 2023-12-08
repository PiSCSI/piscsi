"""Module that implements a button cycling functionality"""
from abc import abstractmethod
from menu.timer import Timer
from piscsi.file_cmds import FileCmds


class Cycler:
    """Class implementing button cycling functionality. Message is shown at the center of
    the screen where repeated button presses cycle through the available selection
    possibilities. Inactivity (cycle_timeout) actives cycle entry last shown on the screen."""

    def __init__(
        self,
        menu_controller,
        sock_cmd,
        piscsi_cmd,
        cycle_timeout=3,
        return_string="Return ->",
        return_entry=True,
        empty_messages=True,
    ):
        self._cycle_profile_timer_flag = Timer(activation_delay=cycle_timeout)
        self._menu_controller = menu_controller
        self.sock_cmd = sock_cmd
        self.piscsi_cmd = piscsi_cmd
        self.file_cmd = FileCmds(piscsi=self.piscsi_cmd)
        self.cycle_entries = self.populate_cycle_entries()
        self.return_string = return_string
        self.return_entry = return_entry
        self.empty_messages = empty_messages
        if self.return_entry is True:
            self.cycle_entries.insert(0, self.return_string)
        self.selected_config_file_index = 0
        self.message = str(self.cycle_entries[self.selected_config_file_index])
        self.update()

    @abstractmethod
    def populate_cycle_entries(self):
        """Returns a list of entries to cycle"""

    @abstractmethod
    def perform_selected_entry_action(self, selected_entry):
        """Performs an action on the selected cycle menu entry"""

    @abstractmethod
    def perform_return_action(self):
        """Perform the return action, i.e., when no selection is chosen"""

    def update(self):
        """Returns True if object has completed its task and can be deleted"""

        if self._cycle_profile_timer_flag is None:
            return None

        self._cycle_profile_timer_flag.check_timer()
        if self.message is not None:
            self._menu_controller.show_timed_mini_message(self.message)
            self._menu_controller.get_menu_renderer().render()

        if self._cycle_profile_timer_flag.enabled is False:  # timer is running
            return None

        selected_cycle_entry = str(self.cycle_entries[self.selected_config_file_index])

        if self.return_entry is True:
            if selected_cycle_entry != self.return_string:
                if self.empty_messages is True:
                    self._menu_controller.show_timed_mini_message("")
                    self._menu_controller.show_timed_message("")
                return self.perform_selected_entry_action(selected_cycle_entry)

            self._menu_controller.show_timed_mini_message("")
            self._menu_controller.show_timed_message("")
            return self.perform_return_action()

        return self.perform_selected_entry_action(selected_cycle_entry)

    def cycle(self):
        """Cycles between entries in the cycle menu"""
        if self._cycle_profile_timer_flag is None:
            return

        self.selected_config_file_index += 1

        if self.selected_config_file_index > len(self.cycle_entries) - 1:
            self.selected_config_file_index = 0

        self.message = str(self.cycle_entries[self.selected_config_file_index])

        self._cycle_profile_timer_flag.reset_timer()
