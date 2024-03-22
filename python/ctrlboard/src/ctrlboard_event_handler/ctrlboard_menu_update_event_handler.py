"""Module for interfacing between the menu controller and the PiSCSI Control Board hardware"""

import logging
from typing import Optional

from ctrlboard_event_handler.piscsi_profile_cycler import PiscsiProfileCycler
from ctrlboard_event_handler.piscsi_shutdown_cycler import PiscsiShutdownCycler
from ctrlboard_menu_builder import CtrlBoardMenuBuilder
from ctrlboard_hw.ctrlboard_hw_constants import CtrlBoardHardwareConstants
from ctrlboard_hw.hardware_button import HardwareButton
from ctrlboard_hw.encoder import Encoder
from observer import Observer
from piscsi.file_cmds import FileCmds
from piscsi.piscsi_cmds import PiscsiCmds
from piscsi.socket_cmds import SocketCmds
from piscsi_menu_controller import PiscsiMenuController


# pylint: disable=too-many-instance-attributes
class CtrlBoardMenuUpdateEventHandler(Observer):
    """Class interfacing the menu controller the PiSCSI Control Board hardware."""

    def __init__(
        self,
        menu_controller: PiscsiMenuController,
        sock_cmd: SocketCmds,
        piscsi_cmd: PiscsiCmds,
    ):
        self.message = None
        self._menu_controller = menu_controller
        self._menu_renderer_config = self._menu_controller.get_menu_renderer().get_config()
        self.sock_cmd = sock_cmd
        self.piscsi_cmd = piscsi_cmd
        self.context_stack = []
        self.piscsi_profile_cycler: Optional[PiscsiProfileCycler] = None
        self.piscsi_shutdown_cycler: Optional[PiscsiShutdownCycler] = None

    def update(self, updated_object):
        if isinstance(updated_object, HardwareButton):
            if updated_object.name == CtrlBoardHardwareConstants.ROTARY_BUTTON:
                menu = self._menu_controller.get_active_menu()
                info_object = menu.get_current_info_object()

                self.route_rotary_button_handler(info_object)

                self._menu_controller.get_menu_renderer().render()
            else:  # button pressed
                if updated_object.name == "Bt1":
                    self.handle_button1()
                elif updated_object.name == "Bt2":
                    self.handle_button2()
        if isinstance(updated_object, Encoder):
            active_menu = self._menu_controller.get_active_menu()
            if updated_object.direction == 1:
                if active_menu.item_selection + 1 < len(active_menu.entries):
                    self._menu_controller.get_active_menu().item_selection += 1
            if updated_object.direction == -1:
                if active_menu.item_selection - 1 >= 0:
                    active_menu.item_selection -= 1
                else:
                    active_menu.item_selection = 0
            self._menu_controller.get_menu_renderer().render()

    def update_events(self):
        """Method handling non-blocking event handling for the cycle buttons."""
        if self.piscsi_profile_cycler is not None:
            result = self.piscsi_profile_cycler.update()
            if result is not None:
                self.piscsi_profile_cycler = None
                self.context_stack = []
                self._menu_controller.segue(result)
        if self.piscsi_shutdown_cycler is not None:
            self.piscsi_shutdown_cycler.empty_messages = False
            result = self.piscsi_shutdown_cycler.update()
            if result == "return":
                self.piscsi_shutdown_cycler = None

    def handle_button1(self):
        """Method for handling the first cycle button (cycle profiles)"""
        if self.piscsi_profile_cycler is None:
            self.piscsi_profile_cycler = PiscsiProfileCycler(
                self._menu_controller, self.sock_cmd, self.piscsi_cmd, return_entry=True
            )
        else:
            self.piscsi_profile_cycler.cycle()

    def handle_button2(self):
        """Method for handling the second cycle button (cycle shutdown)"""
        if self.piscsi_shutdown_cycler is None:
            self.piscsi_shutdown_cycler = PiscsiShutdownCycler(
                self._menu_controller, self.sock_cmd, self.piscsi_cmd
            )
        else:
            self.piscsi_shutdown_cycler.cycle()

    def route_rotary_button_handler(self, info_object):
        """Method for handling the rotary button press for the menu navigation"""
        if info_object is None:
            return

        context = info_object["context"]
        action = info_object["action"]
        handler_function_name = "handle_" + context + "_" + action
        try:
            handler_function = getattr(self, handler_function_name)
            if handler_function is not None:
                handler_function(info_object)
        except AttributeError:
            log = logging.getLogger(__name__)
            log.debug(
                "Handler function [%s] not found or returned an error. Skipping.",
                str(handler_function_name),
            )

    # noinspection PyUnusedLocal
    # pylint: disable=unused-argument
    def handle_scsi_id_menu_openactionmenu(self, info_object):
        """Method handles the rotary button press with the scsi list to open the action menu."""
        context_object = self._menu_controller.get_active_menu().get_current_info_object()
        self.context_stack.append(context_object)
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.ACTION_MENU,
            context_object=context_object,
            transition_attributes=self._menu_renderer_config.transition_attributes_left,
        )

    # noinspection PyUnusedLocal
    # pylint: disable=unused-argument
    def handle_action_menu_return(self, info_object):
        """Method handles the rotary button press to return from the
        action menu to the scsi list."""
        self.context_stack.pop()
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.SCSI_ID_MENU,
            transition_attributes=self._menu_renderer_config.transition_attributes_right,
        )

    # noinspection PyUnusedLocal
    # pylint: disable=unused-argument
    def handle_action_menu_slot_attachinsert(self, info_object):
        """Method handles the rotary button press on attach in the action menu."""
        context_object = self._menu_controller.get_active_menu().context_object
        self.context_stack.append(context_object)
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.IMAGES_MENU,
            context_object=context_object,
            transition_attributes=self._menu_renderer_config.transition_attributes_left,
        )

    # noinspection PyUnusedLocal
    def handle_action_menu_slot_detacheject(self, info_object):
        """Method handles the rotary button press on detach in the action menu."""
        context_object = self._menu_controller.get_active_menu().context_object
        self.detach_eject_scsi_id()
        self.context_stack = []
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.SCSI_ID_MENU,
            context_object=context_object,
            transition_attributes=self._menu_renderer_config.transition_attributes_right,
        )

    # noinspection PyUnusedLocal
    def handle_action_menu_slot_info(self, info_object):
        """Method handles the rotary button press on 'Info' in the action menu."""
        context_object = self._menu_controller.get_active_menu().context_object
        self.context_stack.append(context_object)
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.DEVICEINFO_MENU,
            transition_attributes=self._menu_renderer_config.transition_attributes_left,
            context_object=context_object,
        )

    # noinspection PyUnusedLocal
    def handle_device_info_menu_return(self, info_object):
        """Method handles the rotary button press on 'Return' in the info menu."""
        self.context_stack.pop()
        context_object = self._menu_controller.get_active_menu().context_object
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.ACTION_MENU,
            transition_attributes=self._menu_renderer_config.transition_attributes_right,
            context_object=context_object,
        )

    # noinspection PyUnusedLocal
    def handle_action_menu_loadprofile(self, info_object):
        """Method handles the rotary button press on 'Load Profile' in the action menu."""
        context_object = self._menu_controller.get_active_menu().context_object
        self.context_stack.append(context_object)
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.PROFILES_MENU,
            transition_attributes=self._menu_renderer_config.transition_attributes_left,
        )

    # noinspection PyUnusedLocal
    def handle_profiles_menu_loadprofile(self, info_object):
        """Method handles the rotary button press in the profile selection menu
        for selecting a profile to load."""
        if info_object is not None and "name" in info_object:
            file_cmd = FileCmds(piscsi=self.piscsi_cmd)
            result = file_cmd.read_config(file_name=info_object["name"])
            if result["status"] is True:
                self._menu_controller.show_message("Profile loaded!")
            else:
                self._menu_controller.show_message("Loading failed!")

        self.context_stack = []
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.SCSI_ID_MENU,
            transition_attributes=self._menu_renderer_config.transition_attributes_right,
        )

    # noinspection PyUnusedLocal
    def handle_action_menu_shutdown(self, info_object):
        """Method handles the rotary button press on 'Shutdown' in the action menu."""
        self.piscsi_cmd.shutdown("system")
        self._menu_controller.show_message("Shutting down!", 150)
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.SCSI_ID_MENU,
            transition_attributes=self._menu_renderer_config.transition_attributes_right,
        )

    # noinspection PyUnusedLocal
    def handle_images_menu_return(self, info_object):
        """Method handles the rotary button press on 'Return' in the image selection menu
        (through attach/insert)."""
        context_object = self.context_stack.pop()
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.ACTION_MENU,
            context_object=context_object,
            transition_attributes=self._menu_renderer_config.transition_attributes_right,
        )

    def handle_images_menu_image_attachinsert(self, info_object):
        """Method handles the rotary button press on an image in the image selection menu
        (through attach/insert)"""
        context_object = self._menu_controller.get_active_menu().context_object
        self.attach_insert_scsi_id(info_object)
        self.context_stack = []
        self._menu_controller.segue(
            CtrlBoardMenuBuilder.SCSI_ID_MENU,
            context_object=context_object,
            transition_attributes=self._menu_renderer_config.transition_attributes_right,
        )

    def attach_insert_scsi_id(self, info_object):
        """Helper method to attach/insert an image on a scsi id given through the menu context"""
        image_name = info_object["name"]
        device_type = info_object["device_type"]
        context_object = self._menu_controller.get_active_menu().context_object
        scsi_id = context_object["scsi_id"]
        params = {"file": image_name}
        result = self.piscsi_cmd.attach_device(
            scsi_id=scsi_id, device_type=device_type, params=params
        )

        if result["status"] is False:
            self._menu_controller.show_message("Attach failed!")
        else:
            self.show_id_action_message(scsi_id, "attached")

    def detach_eject_scsi_id(self):
        """Helper method to detach/eject an image on a scsi id given through the menu context"""
        context_object = self._menu_controller.get_active_menu().context_object
        scsi_id = context_object["scsi_id"]
        device_info = self.piscsi_cmd.list_devices(scsi_id)

        if not device_info["device_list"]:
            return

        device_type = device_info["device_list"][0]["device_type"]
        image = device_info["device_list"][0]["image"]
        if device_type in ("SCHD", "SCBR", "SCDP", "SCLP", "SCHS"):
            result = self.piscsi_cmd.detach_by_id(scsi_id)
            if result["status"] is True:
                self.show_id_action_message(scsi_id, "detached")
            else:
                self._menu_controller.show_message("Detach failed!")
        elif device_type in ("SCRM", "SCMO", "SCCD"):
            if image:
                result = self.piscsi_cmd.eject_by_id(scsi_id)
                if result["status"] is True:
                    self.show_id_action_message(scsi_id, "ejected")
                else:
                    self._menu_controller.show_message("Eject failed!")
            else:
                result = self.piscsi_cmd.detach_by_id(scsi_id)
                if result["status"] is True:
                    self.show_id_action_message(scsi_id, "detached")
                else:
                    self._menu_controller.show_message("Detach failed!")
        else:
            log = logging.getLogger(__name__)
            log.info(
                "Device type '%s' currently unsupported for detach/eject!",
                str(device_type),
            )

    def show_id_action_message(self, scsi_id, action: str):
        """Helper method for displaying an action message in the case of an exception."""
        self._menu_controller.show_message("ID " + str(scsi_id) + " " + action + "!")
