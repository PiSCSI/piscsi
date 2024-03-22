"""Module implementing the PiSCSI Control Board UI specific menu controller"""

from ctrlboard_menu_builder import CtrlBoardMenuBuilder
from menu.menu_builder import MenuBuilder
from menu.menu_controller import MenuController
from menu.menu_renderer import MenuRenderer
from menu.timer import Timer


class PiscsiMenuController(MenuController):
    """Class implementing a PiSCSI Control Board UI specific menu controller"""

    def __init__(
        self,
        refresh_interval,
        menu_builder: MenuBuilder,
        menu_renderer=None,
        menu_renderer_config=None,
    ):
        super().__init__(menu_builder, menu_renderer, menu_renderer_config)
        self._refresh_interval = refresh_interval
        self._menu_renderer: MenuRenderer = menu_renderer
        self._scsi_list_refresh_timer_flag = Timer(self._refresh_interval)

    def segue(self, name, context_object=None, transition_attributes=None):
        super().segue(name, context_object, transition_attributes)
        self._scsi_list_refresh_timer_flag.reset_timer()

    def update(self):
        super().update()

        if self.get_active_menu().name == CtrlBoardMenuBuilder.SCSI_ID_MENU:
            self._scsi_list_refresh_timer_flag.check_timer()
            if self._scsi_list_refresh_timer_flag.enabled is True:
                self._scsi_list_refresh_timer_flag.reset_timer()
                self.refresh(name=CtrlBoardMenuBuilder.SCSI_ID_MENU)
                if self._menu_renderer.screensaver.enabled is False:
                    self.set_active_menu(CtrlBoardMenuBuilder.SCSI_ID_MENU, display_on_device=False)
