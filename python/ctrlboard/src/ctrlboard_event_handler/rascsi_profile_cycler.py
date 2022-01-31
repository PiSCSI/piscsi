from ctrlboard_menu_builder import CtrlBoardMenuBuilder
from menu.timer_flag import TimerFlag
from rascsi.file_cmds import FileCmds


class RascsiProfileCycler:

    def __init__(self, menu_controller, sock_cmd, ractl_cmd, cycle_timeout=2):
        self._cycle_profile_timer_flag = TimerFlag(activation_delay=cycle_timeout)
        self._menu_controller = menu_controller
        self.sock_cmd = sock_cmd
        self.ractl_cmd = ractl_cmd
        self.file_cmd = FileCmds(sock_cmd=self.sock_cmd, ractl=self.ractl_cmd)
        self.config_files = self.file_cmd.list_config_files()
        self.selected_config_file_index = 0
        self.message = str(self.config_files[self.selected_config_file_index])
        self.update()

    def update(self):
        """ Returns True if object has completed its task and can be deleted """

        if self._cycle_profile_timer_flag is None:
            return

        self._cycle_profile_timer_flag.check_timer()
        if self.message is not None:
            self._menu_controller.show_timed_mini_message(self.message)
            self._menu_controller.get_menu_renderer().render()

        if self._cycle_profile_timer_flag.enabled is False:  # timer is running
            return

        file_to_load = str(self.config_files[self.selected_config_file_index])
#        print("loading: " + file_to_load)
        result = self.file_cmd.read_config(file_to_load)
        self._menu_controller.show_timed_mini_message("")
        if result["status"] is True:
            return CtrlBoardMenuBuilder.SCSI_ID_MENU

        self._menu_controller.show_message("Failed!")

        return CtrlBoardMenuBuilder.SCSI_ID_MENU

    def cycle(self):
        if self._cycle_profile_timer_flag is None:
            return

        self.selected_config_file_index += 1

        if self.selected_config_file_index > len(self.config_files) - 1:
            self.selected_config_file_index = 0

        self.message = str(self.config_files[self.selected_config_file_index])
#        print("selected: " + str(self.config_files[self.selected_config_file_index]))

        self._cycle_profile_timer_flag.reset_timer()
