from ctrlboard_hw.ctrlboard_hw_constants import CtrlBoardHardwareConstants
from ctrlboard_menu_builder import CtrlBoardMenuBuilder
from menu.menu_controller import MenuController
from ctrlboard_hw.hardware_button import HardwareButton
from ctrlboard_hw.encoder import Encoder
from observer import Observer
from rascsi.file_cmds import FileCmds
from rascsi.ractl_cmds import RaCtlCmds
from rascsi.socket_cmds import SocketCmds


class CtrlBoardMenuUpdateEventHandler(Observer):

    def __init__(self, menu_controller: MenuController, sock_cmd: SocketCmds, ractl_cmd: RaCtlCmds):
        self.message = None
        self._menu_controller = menu_controller
        self._menu_renderer_config = self._menu_controller.get_menu_renderer().get_config()
        self.sock_cmd = sock_cmd
        self.ractl_cmd = ractl_cmd

    def update(self, updated_object):
        if isinstance(updated_object, HardwareButton):
            if updated_object.name == CtrlBoardHardwareConstants.ROTARY_BUTTON:
                menu = self._menu_controller.get_active_menu()
                info_object = menu.get_current_info_object()

                self.route_rotary_button_handler(info_object)

                self._menu_controller.get_menu_renderer().render()
            else:
                self._menu_controller.show_message(updated_object.name + " pressed!")
                self._menu_controller.get_menu_renderer().render()
        if isinstance(updated_object, Encoder):
            # print(updatedObject.direction)
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

    def route_rotary_button_handler(self, info_object):
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
            print("handler function [" + str(handler_function_name) + "] not found. Skipping.")

    # noinspection PyUnusedLocal
    def handle_scsi_id_menu_openactionmenu(self, info_object):
        context_object = self._menu_controller.get_active_menu().get_current_info_object()
        self._menu_controller.segue(CtrlBoardMenuBuilder.ACTION_MENU, context_object=context_object,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_left)

    # noinspection PyUnusedLocal
    def handle_action_menu_return(self, info_object):
        self._menu_controller.segue(CtrlBoardMenuBuilder.SCSI_ID_MENU,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_right)

    # noinspection PyUnusedLocal
    def handle_action_menu_slot_attachinsert(self, info_object):
        context_object = self._menu_controller.get_active_menu().context_object
        scsi_id = context_object["scsi_id"]
        self._menu_controller.segue(CtrlBoardMenuBuilder.IMAGES_MENU, context_object=context_object,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_left)

    # noinspection PyUnusedLocal
    def handle_action_menu_slot_detacheject(self, info_object):
        context_object = self._menu_controller.get_active_menu().context_object
        self.detach_eject_scsi_id()
        self._menu_controller.segue(CtrlBoardMenuBuilder.SCSI_ID_MENU, context_object=context_object,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_left)

    # noinspection PyUnusedLocal
    def handle_action_menu_slot_info(self, info_object):
        context_object = self._menu_controller.get_active_menu().context_object
        self._menu_controller.segue(CtrlBoardMenuBuilder.DEVICEINFO_MENU,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_left,
                                    context_object=context_object)

    # noinspection PyUnusedLocal
    def handle_device_info_menu_return(self, info_object):
        context_object = self._menu_controller.get_active_menu().context_object
        self._menu_controller.segue(CtrlBoardMenuBuilder.ACTION_MENU,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_right,
                                    context_object=context_object)

    # noinspection PyUnusedLocal
    def handle_action_menu_loadprofile(self, info_object):
        self._menu_controller.segue(CtrlBoardMenuBuilder.PROFILES_MENU,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_left)

    # noinspection PyUnusedLocal
    def handle_profiles_menu_loadprofile(self, info_object):
        if info_object is not None and "name" in info_object:
            # file_tools = FileCmds(rascsi_client=self._menu_controller.get_rascsi_client())
            file_cmd = FileCmds(sock_cmd=self.sock_cmd, ractl=self.ractl_cmd)
            file_cmd.read_config(file_name=info_object["name"])
            self._menu_controller.show_message("Profile loaded!")

        self._menu_controller.segue(CtrlBoardMenuBuilder.SCSI_ID_MENU,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_left)

    # noinspection PyUnusedLocal
    def handle_action_menu_shutdown(self, info_object):
        self._menu_controller.get_rascsi_client().shutdown_pi("system")
        self._menu_controller.show_message("Shutting down!", 150)
        self._menu_controller.segue(CtrlBoardMenuBuilder.SCSI_ID_MENU,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_right)

    # noinspection PyUnusedLocal
    def handle_images_menu_return(self, info_object):
        self._menu_controller.segue(CtrlBoardMenuBuilder.ACTION_MENU,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_right)

    def handle_images_menu_image_attachinsert(self, info_object):
        context_object = self._menu_controller.get_active_menu().context_object
        self.attach_insert_scsi_id(info_object)
        self._menu_controller.segue(CtrlBoardMenuBuilder.SCSI_ID_MENU, context_object=context_object,
                                    transition_attributes=self._menu_renderer_config.transition_attributes_left)

    def attach_insert_scsi_id(self, info_object):
        image_name = info_object["name"]
        device_type = info_object["device_type"]
        context_object = self._menu_controller.get_active_menu().context_object
        scsi_id = context_object["scsi_id"]
        rascsi_client = self._menu_controller.get_rascsi_client()
        # TODO: check if device_type == device_type of slot! Currently it exits with exception here if SCRM is
        #  inserted into SCCD
        rascsi_client.attach_image(scsi_id=scsi_id, device_type=device_type, image=image_name)
        # TODO: message should depend on the actual result!
        self.show_id_action_message(scsi_id, "attached")

    def detach_eject_scsi_id(self):
        context_object = self._menu_controller.get_active_menu().context_object
        rascsi_client = self._menu_controller.get_rascsi_client()
        scsi_id = context_object["scsi_id"]
        device_info = rascsi_client.list_devices(scsi_id)

        if len(device_info["device_list"]) == 0:
            return

        device_type = device_info["device_list"][0]["device_type"]
        image = device_info["device_list"][0]["image"]
        # TODO: deal with SCBR and SCDP properly
        if device_type == "SAHD" or device_type == "SCHD":
            result = rascsi_client.detach_by_id(scsi_id)
            self.show_id_action_message(scsi_id, "detached")
        elif device_type == "SCRM" or device_type == "SCMO" or device_type == "SCCD":
            if len(image) > 0:
                result = rascsi_client.eject_by_id(scsi_id)
                # TODO: message should depend on the actual result!
                self.show_id_action_message(scsi_id, "ejected")
            else:
                result = rascsi_client.detach_by_id(scsi_id)
                # TODO: message should depend on the actual result!
                self.show_id_action_message(scsi_id, "detached")
        else:
            # TODO: message should depend on the actual result!
            print("device type currently unhandled!")

    def show_id_action_message(self, scsi_id, action: str):
        self._menu_controller.show_message("ID " + str(scsi_id) + " " + action + "!")

