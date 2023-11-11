"""Module for building the control board UI specific menus"""
import logging

from menu.menu import Menu
from menu.menu_builder import MenuBuilder
from piscsi.file_cmds import FileCmds
from piscsi.piscsi_cmds import PiscsiCmds


class CtrlBoardMenuBuilder(MenuBuilder):
    """Class for building the control board UI specific menus"""

    SCSI_ID_MENU = "scsi_id_menu"
    ACTION_MENU = "action_menu"
    IMAGES_MENU = "images_menu"
    PROFILES_MENU = "profiles_menu"
    DEVICEINFO_MENU = "device_info_menu"

    ACTION_OPENACTIONMENU = "openactionmenu"
    ACTION_RETURN = "return"
    ACTION_SLOT_ATTACHINSERT = "slot_attachinsert"
    ACTION_SLOT_DETACHEJECT = "slot_detacheject"
    ACTION_SLOT_INFO = "slot_info"
    ACTION_SHUTDOWN = "shutdown"
    ACTION_LOADPROFILE = "loadprofile"
    ACTION_IMAGE_ATTACHINSERT = "image_attachinsert"

    def __init__(self, piscsi_cmd: PiscsiCmds):
        super().__init__()
        self._piscsi_client = piscsi_cmd
        self.file_cmd = FileCmds(
            sock_cmd=piscsi_cmd.sock_cmd,
            piscsi=piscsi_cmd,
            token=piscsi_cmd.token,
            locale=piscsi_cmd.locale,
        )

    def build(self, name: str, context_object=None) -> Menu:
        if name == CtrlBoardMenuBuilder.SCSI_ID_MENU:
            return self.create_scsi_id_list_menu(context_object)
        if name == CtrlBoardMenuBuilder.ACTION_MENU:
            return self.create_action_menu(context_object)
        if name == CtrlBoardMenuBuilder.IMAGES_MENU:
            return self.create_images_menu(context_object)
        if name == CtrlBoardMenuBuilder.PROFILES_MENU:
            return self.create_profiles_menu(context_object)
        if name == CtrlBoardMenuBuilder.DEVICEINFO_MENU:
            return self.create_device_info_menu(context_object)

        log = logging.getLogger(__name__)
        log.error("Provided menu name cannot be built!")

        return self.create_scsi_id_list_menu(context_object)

    # pylint: disable=unused-argument
    def create_scsi_id_list_menu(self, context_object=None):
        """Method creates the menu displaying the 7 scsi slots"""
        devices = self._piscsi_client.list_devices()
        reserved_ids = self._piscsi_client.get_reserved_ids()

        devices_by_id = {}
        for device in devices["device_list"]:
            devices_by_id[int(device["id"])] = device

        menu = Menu(CtrlBoardMenuBuilder.SCSI_ID_MENU)

        if reserved_ids["status"] is False:
            menu.add_entry("No scsi ids reserved")

        for scsi_id in range(0, 8):
            device = None
            if devices_by_id.get(scsi_id) is not None:
                device = devices_by_id[scsi_id]
            file = "-"
            device_type = ""

            if str(scsi_id) in reserved_ids["ids"]:
                file = "[Reserved]"
            elif device is not None:
                file = str(device["file"])
                device_type = str(device["device_type"])

            menu_str = str(scsi_id) + ":"

            if device_type == "SCDP":
                menu_str += "Daynaport"
            elif device_type == "SCBR":
                menu_str += "X68000 Host Bridge"
            elif device_type == "SCLP":
                menu_str += "SCSI Printer"
            elif device_type == "SCHS":
                menu_str += "Host Services"
            else:
                if file == "":
                    menu_str += "(empty)"
                else:
                    menu_str += file
                if device_type != "":
                    menu_str += " [" + device_type + "]"

            menu.add_entry(
                menu_str,
                {
                    "context": self.SCSI_ID_MENU,
                    "action": self.ACTION_OPENACTIONMENU,
                    "scsi_id": scsi_id,
                },
            )

        return menu

    # noinspection PyMethodMayBeStatic
    def create_action_menu(self, context_object=None):
        """Method creates the action submenu with action that can be performed on a scsi slot"""
        menu = Menu(CtrlBoardMenuBuilder.ACTION_MENU)
        menu.add_entry(
            "Return",
            {"context": self.ACTION_MENU, "action": self.ACTION_RETURN},
        )
        menu.add_entry(
            "Attach/Insert",
            {"context": self.ACTION_MENU, "action": self.ACTION_SLOT_ATTACHINSERT},
        )
        menu.add_entry(
            "Detach/Eject",
            {"context": self.ACTION_MENU, "action": self.ACTION_SLOT_DETACHEJECT},
        )
        menu.add_entry(
            "Info",
            {"context": self.ACTION_MENU, "action": self.ACTION_SLOT_INFO},
        )
        menu.add_entry(
            "Load Profile",
            {"context": self.ACTION_MENU, "action": self.ACTION_LOADPROFILE},
        )
        menu.add_entry(
            "Shutdown",
            {"context": self.ACTION_MENU, "action": self.ACTION_SHUTDOWN},
        )
        return menu

    def create_images_menu(self, context_object=None):
        """Creates a sub menu showing all the available images"""
        menu = Menu(CtrlBoardMenuBuilder.IMAGES_MENU)
        images_info = self.piscsi_cmd.list_images()
        menu.add_entry("Return", {"context": self.IMAGES_MENU, "action": self.ACTION_RETURN})
        images = images_info["files"]
        sorted_images = sorted(images, key=lambda d: d["name"])
        for image in sorted_images:
            image_str = image["name"] + " [" + image["detected_type"] + "]"
            image_context = {
                "context": self.IMAGES_MENU,
                "name": str(image["name"]),
                "device_type": str(image["detected_type"]),
                "action": self.ACTION_IMAGE_ATTACHINSERT,
            }
            menu.add_entry(image_str, image_context)
        return menu

    def create_profiles_menu(self, context_object=None):
        """Creates a sub menu showing all the available profiles"""
        menu = Menu(CtrlBoardMenuBuilder.PROFILES_MENU)
        menu.add_entry("Return", {"context": self.IMAGES_MENU, "action": self.ACTION_RETURN})
        config_files = self.file_cmd.list_config_files()
        for config_file in config_files:
            menu.add_entry(
                str(config_file),
                {
                    "context": self.PROFILES_MENU,
                    "name": str(config_file),
                    "action": self.ACTION_LOADPROFILE,
                },
            )

        return menu

    def create_device_info_menu(self, context_object=None):
        """Create a menu displaying information of an image in a scsi slot"""
        menu = Menu(CtrlBoardMenuBuilder.DEVICEINFO_MENU)
        menu.add_entry("Return", {"context": self.DEVICEINFO_MENU, "action": self.ACTION_RETURN})

        device_info = self._piscsi_client.list_devices(context_object["scsi_id"])

        if not device_info["device_list"]:
            return menu

        scsi_id = context_object["scsi_id"]
        file = device_info["device_list"][0]["file"]
        status = device_info["device_list"][0]["status"]
        if not status:
            status = "Read/Write"
        lun = device_info["device_list"][0]["unit"]
        device_type = device_info["device_list"][0]["device_type"]
        if "parameters" in device_info["device_list"][0]:
            parameters = device_info["device_list"][0]["parameters"]
        else:
            parameters = "{}"

        menu.add_entry("ID   : " + str(scsi_id))
        menu.add_entry("LUN  : " + str(lun))
        menu.add_entry("File : " + str(file))
        menu.add_entry("Type : " + str(device_type))
        menu.add_entry("R/RW : " + str(status))
        menu.add_entry("Prms : " + str(parameters))
        menu.add_entry("Vndr : " + str(device_info["device_list"][0]["vendor"]))
        menu.add_entry("Prdct: " + str(device_info["device_list"][0]["product"]))
        menu.add_entry("Rvisn: " + str(device_info["device_list"][0]["revision"]))
        menu.add_entry("Blksz: " + str(device_info["device_list"][0]["block_size"]))
        menu.add_entry("Imgsz: " + str(device_info["device_list"][0]["size"]))

        return menu

    def get_piscsi_client(self):
        """Returns an instance of the piscsi client"""
        return self._piscsi_client
