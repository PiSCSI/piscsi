from menu.cycler import Cycler


class RascsiShutdownCycler(Cycler):

    def __init__(self, menu_controller, sock_cmd, ractl_cmd):
        super().__init__(menu_controller, sock_cmd, ractl_cmd, return_entry=True, empty_messages=False)
        self.executed_once = False

    def populate_cycle_entries(self):
        cycle_entries = ["Shutdown"]

        return cycle_entries

    def perform_selected_entry_action(self, selected_entry):
        if self.executed_once is False:
            self.executed_once = True
            self._menu_controller.show_timed_message("Shutting down...")
            self.ractl_cmd.shutdown_pi("system")
            return "shutdown"
        else:
            return None

    def perform_return_action(self):
        self._menu_controller.show_timed_mini_message("")
        self._menu_controller.show_timed_message("")

        return "return"
