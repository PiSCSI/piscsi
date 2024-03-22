"""Module is the entry point for the PiSCSI Control Board UI"""

import argparse
import sys
import logging

from config import CtrlboardConfig
from ctrlboard_hw.ctrlboard_hw import CtrlBoardHardware
from ctrlboard_hw.ctrlboard_hw_constants import CtrlBoardHardwareConstants
from ctrlboard_event_handler.ctrlboard_menu_update_event_handler import (
    CtrlBoardMenuUpdateEventHandler,
)
from ctrlboard_menu_builder import CtrlBoardMenuBuilder
from menu.menu_renderer_config import MenuRendererConfig
from menu.menu_renderer_luma_oled import MenuRendererLumaOled
from piscsi.exceptions import (
    EmptySocketChunkException,
    InvalidProtobufResponse,
    FailedSocketConnectionException,
)
from piscsi.piscsi_cmds import PiscsiCmds
from piscsi.socket_cmds import SocketCmds

from piscsi_menu_controller import PiscsiMenuController


def parse_config():
    """Parses the command line parameters and configured the PiSCSI Control Board UI accordingly"""
    config = CtrlboardConfig()
    cmdline_args_parser = argparse.ArgumentParser(description="PiSCSI ctrlboard service")
    cmdline_args_parser.add_argument(
        "--rotation",
        type=int,
        choices=[0, 90, 180, 270],
        default=180,
        action="store",
        help="The rotation of the screen buffer in degrees. Default: 180",
    )
    cmdline_args_parser.add_argument(
        "--height",
        type=int,
        choices=[64],
        default=64,
        action="store",
        help="The pixel height of the screen buffer. Default: 64",
    )
    cmdline_args_parser.add_argument(
        "--piscsi-host",
        type=str,
        default="localhost",
        action="store",
        help="PiSCSI host. Default: localhost",
    )
    cmdline_args_parser.add_argument(
        "--piscsi-port",
        type=int,
        default=6868,
        action="store",
        help="PiSCSI port. Default: 6868",
    )
    cmdline_args_parser.add_argument(
        "--password",
        type=str,
        default="",
        action="store",
        help="Token password string for authenticating with PiSCSI",
    )
    cmdline_args_parser.add_argument(
        "--loglevel",
        type=int,
        choices=[0, 10, 30, 40, 50],
        default=logging.WARN,
        action="store",
        help="Loglevel. Valid values: 0 (notset), 10 (debug), 30 (warning), "
        "40 (error), 50 (critical). Default: Warning",
    )
    cmdline_args_parser.add_argument(
        "--transitions",
        type=int,
        choices=[0, 1],
        default=1,
        action="store",
        help="Transition animations. Valid values: 0 (disabled), 1 (enabled). Default: 1",
    )
    args = cmdline_args_parser.parse_args()
    config.ROTATION = args.rotation

    if args.height == 64:
        config.HEIGHT = 64
        config.LINES = 8
    elif args.height == 32:
        config.HEIGHT = 32
        config.LINES = 4
    config.TOKEN = args.password
    config.WIDTH = 128
    config.BORDER = 5
    config.PISCSI_HOST = args.piscsi_host
    config.PISCSI_PORT = args.piscsi_port
    config.LOG_LEVEL = args.loglevel
    config.TRANSITIONS = bool(args.transitions)

    return config


def check_piscsi_connection(piscsi_cmd):
    """Checks whether a PiSCSI connection exists by polling the PiSCSI server info.
    Returns true if connection works, false if connection fails."""
    try:
        info = piscsi_cmd.get_reserved_ids()
        return bool(info["status"] is True)
    except FailedSocketConnectionException:
        log = logging.getLogger(__name__)
        log.error("Could not establish connection. Stopping service")
        exit(1)


def main():
    """Main function for the PiSCSI Control Board UI"""
    config = parse_config()

    log_format = "%(asctime)s:%(name)s:%(levelname)s - %(message)s"
    logging.basicConfig(stream=sys.stdout, format=log_format, level=config.LOG_LEVEL)
    log = logging.getLogger(__name__)
    log.debug("PiSCSI ctrlboard service started.")

    ctrlboard_hw = CtrlBoardHardware(
        display_i2c_address=config.DISPLAY_I2C_ADDRESS,
        pca9554_i2c_address=config.PCA9554_I2C_ADDRESS,
    )

    # for now, we require the complete piscsi ctrlboard hardware.
    # Oled only will be supported as well at some later point in time.
    if ctrlboard_hw.piscsi_controlboard_detected is False:
        log.error("Ctrlboard hardware not detected. Stopping service")
        exit(1)

    sock_cmd = None
    piscsi_cmd = None
    try:
        sock_cmd = SocketCmds(host=config.PISCSI_HOST, port=config.PISCSI_PORT)
        piscsi_cmd = PiscsiCmds(sock_cmd=sock_cmd, token=config.TOKEN)
    except EmptySocketChunkException:
        log.error("Retrieved empty data from PiSCSI. Stopping service")
        exit(1)
    except InvalidProtobufResponse:
        log.error("Retrieved unexpected data from PiSCSI. Stopping service")
        exit(1)

    if check_piscsi_connection(piscsi_cmd) is False:
        log.error(
            "Communication with PiSCSI failed. Please check if password token must be set "
            "and whether is set correctly."
        )
        exit(1)

    menu_renderer_config = MenuRendererConfig()

    if config.TRANSITIONS is False:
        menu_renderer_config.transition = None

    menu_renderer_config.i2c_address = CtrlBoardHardwareConstants.DISPLAY_I2C_ADDRESS
    menu_renderer_config.rotation = config.ROTATION

    menu_builder = CtrlBoardMenuBuilder(piscsi_cmd)
    menu_controller = PiscsiMenuController(
        config.MENU_REFRESH_INTERVAL,
        menu_builder=menu_builder,
        menu_renderer=MenuRendererLumaOled(menu_renderer_config),
        menu_renderer_config=menu_renderer_config,
    )

    menu_controller.add(CtrlBoardMenuBuilder.SCSI_ID_MENU)
    menu_controller.add(CtrlBoardMenuBuilder.ACTION_MENU)

    menu_controller.show_splash_screen("resources/splash_start_64.bmp")

    menu_update_event_handler = CtrlBoardMenuUpdateEventHandler(
        menu_controller, sock_cmd=sock_cmd, piscsi_cmd=piscsi_cmd
    )
    ctrlboard_hw.attach(menu_update_event_handler)
    menu_controller.set_active_menu(CtrlBoardMenuBuilder.SCSI_ID_MENU)

    while True:
        # pylint: disable=broad-except
        try:
            ctrlboard_hw.process_events()
            menu_update_event_handler.update_events()
            menu_controller.update()
        except KeyboardInterrupt:
            ctrlboard_hw.cleanup()
            break
        except Exception as ex:
            print(ex)


if __name__ == "__main__":
    main()
