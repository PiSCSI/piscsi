# from rascsi.file_cmds import FileTools
from menu.menu import Menu
from menu.menu_builder import MenuBuilder
from rascsi.file_cmds import FileCmds
from rascsi.ractl_cmds import RaCtlCmds


class CtrlBoardMenuBuilder(MenuBuilder):
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

    def __init__(self, ractl_cmd: RaCtlCmds):
        super().__init__(ractl_cmd)
        self.file_cmd = FileCmds(sock_cmd=ractl_cmd.sock_cmd, ractl=ractl_cmd)

    def build(self, name: str, context_object=None) -> Menu:
        if name == CtrlBoardMenuBuilder.SCSI_ID_MENU:
            return self.create_scsi_id_list_menu(context_object)
        elif name == CtrlBoardMenuBuilder.ACTION_MENU:
            return self.create_action_menu(context_object)
        elif name == CtrlBoardMenuBuilder.IMAGES_MENU:
            return self.create_images_menu(context_object)
        elif name == CtrlBoardMenuBuilder.PROFILES_MENU:
            return self.create_profiles_menu(context_object)
        elif name == CtrlBoardMenuBuilder.DEVICEINFO_MENU:
            return self.create_device_info_menu(context_object)
        else:
            print("Provided menu name [" + name + "] cannot be built!")

    def create_scsi_id_list_menu(self, context_object=None):
        devices = self._rascsi_client.list_devices()
        reserved_ids = self._rascsi_client.get_reserved_ids()

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
                menu_str += "Host Bridge"
            else:
                if file == "":
                    menu_str += "(empty)"
                else:
                    menu_str += file
                if device_type != "":
                    menu_str += " [" + device_type + "]"

            menu.add_entry(menu_str, {"context": self.SCSI_ID_MENU,
                                      "action": self.ACTION_OPENACTIONMENU,
                                      "scsi_id": scsi_id})

        return menu

    # noinspection PyMethodMayBeStatic
    def create_action_menu(self, context_object=None):
        menu = Menu(CtrlBoardMenuBuilder.ACTION_MENU)
        menu.add_entry("Return", {"context": self.ACTION_MENU, "action": self.ACTION_RETURN})
        menu.add_entry("Attach/Insert", {"context": self.ACTION_MENU, "action": self.ACTION_SLOT_ATTACHINSERT})
        menu.add_entry("Detach/Eject", {"context": self.ACTION_MENU, "action": self.ACTION_SLOT_DETACHEJECT})
        menu.add_entry("Info", {"context": self.ACTION_MENU, "action": self.ACTION_SLOT_INFO})
        menu.add_entry("Load Profile", {"context": self.ACTION_MENU, "action": self.ACTION_LOADPROFILE})
        menu.add_entry("Shutdown", {"context": self.ACTION_MENU, "action": self.ACTION_SHUTDOWN})
        return menu

    def create_images_menu(self, context_object=None):
        menu = Menu(CtrlBoardMenuBuilder.IMAGES_MENU)
        images_info = self._rascsi_client.get_image_files_info()
        menu.add_entry("Return", {"context": self.IMAGES_MENU, "action": self.ACTION_RETURN})
        images = images_info["image_files"]
        device_types = self.get_rascsi_client().get_device_types()
        for image in images:
            menu.add_entry(str(image.name) + " [" + str(device_types["device_types"][int(image.type)-1]) + "]",
                           {"context": self.IMAGES_MENU, "name": str(image.name),
                            "device_type": str(device_types["device_types"][int(image.type)-1]),
                            "action": self.ACTION_IMAGE_ATTACHINSERT})

        return menu

    def create_profiles_menu(self, context_object=None):
        menu = Menu(CtrlBoardMenuBuilder.PROFILES_MENU)
        menu.add_entry("Return", {"context": self.IMAGES_MENU, "action": self.ACTION_RETURN})
        config_files = self.file_cmd.list_config_files()
        for config_file in config_files:
            menu.add_entry(str(config_file),
                           {"context": self.PROFILES_MENU, "name": str(config_file),
                            "action": self.ACTION_LOADPROFILE})

        return menu

    def create_device_info_menu(self, context_object=None):
        menu = Menu(CtrlBoardMenuBuilder.DEVICEINFO_MENU)
        menu.add_entry("Return", {"context": self.DEVICEINFO_MENU, "action": self.ACTION_RETURN})

        device_info = self._rascsi_client.list_devices(context_object["scsi_id"])
#        print(device_info)
        if len(device_info["device_list"]) <= 0:
            return menu

        scsi_id = context_object["scsi_id"]
        file = device_info["device_list"][0]["file"]
        status = device_info["device_list"][0]["status"]
        if len(status) <= 0:
            status = "Read/Write"
        lun = device_info["device_list"][0]["unit"]
        device_type = device_info["device_list"][0]["device_type"]
        if "parameters" in device_info["device_list"][0]:
            parameters = device_info["device_list"][0]["parameters"]
        else:
            parameters = "{}"
        vendor = device_info["device_list"][0]["vendor"]
        product = device_info["device_list"][0]["product"]
        revision = device_info["device_list"][0]["revision"]
        block_size = device_info["device_list"][0]["block_size"]
        image_size = device_info["device_list"][0]["size"]

        menu.add_entry("ID   : " + str(scsi_id))
        menu.add_entry("LUN  : " + str(lun))
        menu.add_entry("File : " + str(file))
        menu.add_entry("Type : " + str(device_type))
        menu.add_entry("R/RW : " + str(status))
        menu.add_entry("Prms : " + str(parameters))
        menu.add_entry("Vndr : " + str(vendor))
        menu.add_entry("Prdct: " + str(product))
        menu.add_entry("Rvisn: " + str(revision))
        menu.add_entry("Blksz: " + str(block_size))
        menu.add_entry("Imgsz: " + str(image_size))

        return menu
