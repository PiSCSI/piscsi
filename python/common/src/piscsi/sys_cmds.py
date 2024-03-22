"""
Module with methods that interact with the Pi system
"""

import subprocess
import logging
import sys
from subprocess import run, CalledProcessError
from shutil import disk_usage
from re import findall, match
from socket import socket, gethostname, AF_INET, SOCK_DGRAM
from pathlib import Path
from platform import uname

try:
    from vcgencmd import Vcgencmd
except ImportError:
    pass

from piscsi.return_codes import ReturnCodes
from piscsi.common_settings import SHELL_ERROR


class SysCmds:
    """
    Class for commands sent to the Pi's Linux system.
    """

    @staticmethod
    def running_env():
        """
        Returns (str) env, with details on the system hardware and software
        """

        PROC_MODEL_PATH = "/proc/device-tree/model"
        SYS_VENDOR_PATH = "/sys/devices/virtual/dmi/id/sys_vendor"
        SYS_PROD_PATH = "/sys/devices/virtual/dmi/id/product_name"
        # First we try to get the Pi model
        if Path(PROC_MODEL_PATH).is_file():
            try:
                with open(PROC_MODEL_PATH, "r") as open_file:
                    hardware = open_file.read().rstrip()
            except (IOError, ValueError, EOFError, TypeError) as error:
                logging.error(str(error))
        # As a fallback, look for PC vendor information
        elif Path(SYS_VENDOR_PATH).is_file() and Path(SYS_PROD_PATH).is_file():
            hardware = ""
            try:
                with open(SYS_VENDOR_PATH, "r") as open_file:
                    hardware = open_file.read().rstrip() + " "
            except (IOError, ValueError, EOFError, TypeError) as error:
                logging.error(str(error))
            try:
                with open(SYS_PROD_PATH, "r") as open_file:
                    hardware = hardware + open_file.read().rstrip()
            except (IOError, ValueError, EOFError, TypeError) as error:
                logging.error(str(error))
        else:
            hardware = "Unknown Device"

        env = uname()
        return {
            "env": f"{hardware}, {env.system} {env.release} {env.machine}",
        }

    @staticmethod
    def running_proc(daemon):
        """
        Takes (str) daemon
        Returns (int) proc, which is the number of processes currently running
        """
        try:
            processes = (
                subprocess.run(
                    ["ps", "aux"],
                    capture_output=True,
                    check=True,
                )
                .stdout.decode("utf-8")
                .strip()
            )
        except subprocess.CalledProcessError as error:
            logging.warning(SHELL_ERROR, error.cmd, error.stderr.decode("utf-8"))
            processes = ""

        matching_processes = findall(daemon, processes)
        return len(matching_processes)

    @staticmethod
    def is_bridge_setup():
        """
        Returns (bool) True if the piscsi_bridge network interface exists
        """
        try:
            bridges = (
                subprocess.run(
                    ["brctl", "show"],
                    capture_output=True,
                    check=True,
                )
                .stdout.decode("utf-8")
                .strip()
            )
        except subprocess.CalledProcessError as error:
            logging.warning(SHELL_ERROR, error.cmd, error.stderr.decode("utf-8"))
            bridges = ""

        if "piscsi_bridge" in bridges:
            return True
        return False

    @staticmethod
    def disk_space(path):
        """
        Takes (str) path with the path to the dir / mount point to inspect
        Returns a (dict) with (int) total (int) used (int) free disk space
        """
        total, used, free = disk_usage(path)
        return {"total": total, "used": used, "free": free}

    @staticmethod
    def introspect_file(file_path, re_term):
        """
        Takes a (str) file_path and (str) re_term in regex format
        Will introspect file_path for the existance of re_term
        and return True if found, False if not found
        """
        result = False
        try:
            ifile = open(file_path, "r", encoding="ISO-8859-1")
            for line in ifile:
                if match(re_term, line):
                    result = True
                    break
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
        finally:
            ifile.close()
        return result

    # pylint: disable=broad-except
    @staticmethod
    def get_ip_and_host():
        """
        Use a mock socket connection to identify the Pi's hostname and IP address
        """
        host = gethostname()
        sock = socket(AF_INET, SOCK_DGRAM)
        try:
            # mock ip address; doesn't have to be reachable
            sock.connect(("10.255.255.255", 1))
            ip_addr = sock.getsockname()[0]
        except Exception:
            ip_addr = False
        finally:
            sock.close()
        return ip_addr, host

    @staticmethod
    def get_pretty_host():
        """
        Returns either the pretty hostname if set, or the regular hostname as fallback.
        """
        try:
            process = run(
                ["hostnamectl", "status", "--pretty"],
                capture_output=True,
                check=True,
            )
            pretty_hostname = process.stdout.decode("utf-8").rstrip()
            if pretty_hostname:
                return pretty_hostname
        except CalledProcessError as error:
            logging.error(str(error))

        return gethostname()

    @staticmethod
    def set_pretty_host(name):
        """
        Set the pretty hostname for the system
        """
        try:
            run(
                ["sudo", "hostnamectl", "set-hostname", "--pretty", name],
                capture_output=False,
                check=True,
            )
        except CalledProcessError as error:
            logging.error(str(error))
            return False

        return True

    @staticmethod
    def get_logs(lines, scope):
        """
        Takes (int) lines and (str) scope.
        If scope is a None equivalent, all syslogs are returned.
        Returns either the decoded log output, or the stderr output of journalctl.
        """
        line_param = []
        scope_param = []
        if lines:
            line_param = ["-n", lines]
        if scope:
            scope_param = ["-u", scope]
        process = run(
            ["journalctl"] + line_param + scope_param,
            capture_output=True,
        )
        if process.returncode == 0:
            return process.returncode, process.stdout.decode("utf-8")

        return process.returncode, process.stderr.decode("utf-8")

    @staticmethod
    def get_diskinfo(file_path):
        """
        Takes (str) file_path path to image file to inspect.
        Returns either the disktype output, or the stderr output.
        """
        process = run(
            ["disktype", file_path],
            capture_output=True,
        )
        if process.returncode == 0:
            return process.returncode, process.stdout.decode("utf-8")

        return process.returncode, process.stderr.decode("utf-8")

    @staticmethod
    def get_manpage(file_path):
        """
        Takes (str) file_path path to image file to generate manpage for.
        Returns either the man2html output, or the stderr output.
        """
        process = run(
            ["man2html", file_path, "-M", "/"],
            capture_output=True,
        )
        if process.returncode == 0:
            return process.returncode, process.stdout.decode("utf-8")

        return process.returncode, process.stderr.decode("utf-8")

    @staticmethod
    def reboot_system():
        """
        Sends a reboot command to the system
        """
        process = run(
            ["sudo", "reboot"],
            capture_output=True,
        )
        if process.returncode == 0:
            return process.returncode, process.stdout.decode("utf-8")

        return process.returncode, process.stderr.decode("utf-8")

    @staticmethod
    def shutdown_system():
        """
        Sends a shutdown command to the system
        """
        process = run(
            ["sudo", "shutdown", "-h", "now"],
            capture_output=True,
        )
        if process.returncode == 0:
            return process.returncode, process.stdout.decode("utf-8")

        return process.returncode, process.stderr.decode("utf-8")

    @staticmethod
    def get_throttled(enabled_modes, test_modes):
        """
        Takes (list) enabled_modes & (list) test_modes parameters & returns a
        tuple of (str) category & (str) message.

        enabled_modes is a list of modes that will be enabled for display if
        they're triggered.  test_modes works similarly to enabled_mode but will
        ALWAYS display the modes listed for troubleshooting styling.
        """
        if "vcgencmd" in sys.modules:
            vcgcmd = Vcgencmd()
            t_states = vcgcmd.get_throttled()["breakdown"]
            matched_states = []

            state_msgs = {
                "0": ("error", ReturnCodes.UNDER_VOLTAGE_DETECTED),
                "1": ("warning", ReturnCodes.ARM_FREQUENCY_CAPPED),
                "2": ("error", ReturnCodes.CURRENTLY_THROTTLED),
                "3": ("warning", ReturnCodes.SOFT_TEMPERATURE_LIMIT_ACTIVE),
                "16": ("warning", ReturnCodes.UNDER_VOLTAGE_HAS_OCCURRED),
                "17": ("warning", ReturnCodes.ARM_FREQUENCY_CAPPING_HAS_OCCURRED),
                "18": ("warning", ReturnCodes.THROTTLING_HAS_OCCURRED),
                "19": ("warning", ReturnCodes.SOFT_TEMPERATURE_LIMIT_HAS_OCCURRED),
            }

            for k in t_states:
                if t_states[k] and k in enabled_modes:
                    matched_states.append(state_msgs[k])

            for t in test_modes:
                matched_states.append(state_msgs[t])

            return matched_states
        else:
            return []
