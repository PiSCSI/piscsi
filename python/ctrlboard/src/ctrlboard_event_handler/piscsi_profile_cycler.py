"""Module providing the profile cycler class for the PiSCSI Control Board UI"""

from ctrlboard_menu_builder import CtrlBoardMenuBuilder
from menu.cycler import Cycler


class PiscsiProfileCycler(Cycler):
    """Class implementing the profile cycler for the PiSCSI Control Baord UI"""

    def populate_cycle_entries(self):
        cycle_entries = self.file_cmd.list_config_files()

        return cycle_entries

    def perform_selected_entry_action(self, selected_entry):
        result = self.file_cmd.read_config(selected_entry)
        self._menu_controller.show_timed_mini_message("")
        if result["status"] is True:
            return CtrlBoardMenuBuilder.SCSI_ID_MENU

        self._menu_controller.show_message("Failed!")
        return CtrlBoardMenuBuilder.SCSI_ID_MENU

    def perform_return_action(self):
        return CtrlBoardMenuBuilder.SCSI_ID_MENU
