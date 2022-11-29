"""Module is the entry point for the RaSCSI Control Board UI"""
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
from rascsi.exceptions import (
    EmptySocketChunkException,
    InvalidProtobufResponse,
    FailedSocketConnectionException,
)
from rascsi.ractl_cmds import RaCtlCmds
from rascsi.socket_cmds import SocketCmds

from rascsi_menu_controller import RascsiMenuController


def parse_config():
    """Parses the command line parameters and configured the RaSCSI Control Board UI accordingly"""
    config = CtrlboardConfig()
    cmdline_args_parser = argparse.ArgumentParser(description="RaSCSI ctrlboard service")
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
        "--rascsi-host",
        type=str,
        default="localhost",
        action="store",
        help="RaSCSI host. Default: localhost",
    )
    cmdline_args_parser.add_argument(
        "--rascsi-port",
        type=int,
        default=6868,
        action="store",
        help="RaSCSI port. Default: 6868",
    )
    cmdline_args_parser.add_argument(
        "--password",
        type=str,
        default="",
        action="store",
        help="Token password string for authenticating with RaSCSI",
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
    config.RASCSI_HOST = args.rascsi_host
    config.RASCSI_PORT = args.rascsi_port
    config.LOG_LEVEL = args.loglevel
    config.TRANSITIONS = bool(args.transitions)

    return config


def check_rascsi_connection(ractl_cmd):
    """Checks whether a RaSCSI connection exists by polling the RaSCSI server info.
    Returns true if connection works, false if connection fails."""
    try:
        info = ractl_cmd.get_reserved_ids()
        return bool(info["status"] is True)
    except FailedSocketConnectionException:
        log = logging.getLogger(__name__)
        log.error("Could not establish connection. Stopping service")
        exit(1)


def main():
    """Main function for the RaSCSI Control Board UI"""
    config = parse_config()

    log_format = "%(asctime)s:%(name)s:%(levelname)s - %(message)s"
    logging.basicConfig(stream=sys.stdout, format=log_format, level=config.LOG_LEVEL)
    log = logging.getLogger(__name__)
    log.debug("RaSCSI ctrlboard service started.")

    ctrlboard_hw = CtrlBoardHardware(
        display_i2c_address=config.DISPLAY_I2C_ADDRESS,
        pca9554_i2c_address=config.PCA9554_I2C_ADDRESS,
    )

    # for now, we require the complete rascsi ctrlboard hardware.
    # Oled only will be supported as well at some later point in time.
    if ctrlboard_hw.rascsi_controlboard_detected is False:
        log.error("Ctrlboard hardware not detected. Stopping service")
        exit(1)

    sock_cmd = None
    ractl_cmd = None
    try:
        sock_cmd = SocketCmds(host=config.RASCSI_HOST, port=config.RASCSI_PORT)
        ractl_cmd = RaCtlCmds(sock_cmd=sock_cmd, token=config.TOKEN)
    except EmptySocketChunkException:
        log.error("Retrieved empty data from RaSCSI. Stopping service")
        exit(1)
    except InvalidProtobufResponse:
        log.error("Retrieved unexpected data from RaSCSI. Stopping service")
        exit(1)

    if check_rascsi_connection(ractl_cmd) is False:
        log.error(
            "Communication with RaSCSI failed. Please check if password token must be set "
            "and whether is set correctly."
        )
        exit(1)

    menu_renderer_config = MenuRendererConfig()

    if config.TRANSITIONS is False:
        menu_renderer_config.transition = None

    menu_renderer_config.i2c_address = CtrlBoardHardwareConstants.DISPLAY_I2C_ADDRESS
    menu_renderer_config.rotation = config.ROTATION

    menu_builder = CtrlBoardMenuBuilder(ractl_cmd)
    menu_controller = RascsiMenuController(
        config.MENU_REFRESH_INTERVAL,
        menu_builder=menu_builder,
        menu_renderer=MenuRendererLumaOled(menu_renderer_config),
        menu_renderer_config=menu_renderer_config,
    )

    menu_controller.add(CtrlBoardMenuBuilder.SCSI_ID_MENU)
    menu_controller.add(CtrlBoardMenuBuilder.ACTION_MENU)

    menu_controller.show_splash_screen("resources/splash_start_64.bmp")

    menu_update_event_handler = CtrlBoardMenuUpdateEventHandler(
        menu_controller, sock_cmd=sock_cmd, ractl_cmd=ractl_cmd
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
