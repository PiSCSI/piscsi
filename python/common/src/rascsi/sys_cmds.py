"""
Module with methods that interact with the Pi system
"""
import subprocess
import logging
from subprocess import run
from shutil import disk_usage
from re import findall, match
from socket import socket, gethostname, AF_INET, SOCK_DGRAM

ERROR_FORMAT = "[%s] %s"

class SysCmds:
    """
    Class for commands sent to the Pi's Linux system.
    """

    @staticmethod
    def running_env():
        """
        Returns (str) git and (str) env
        git contains the git hash of the checked out code
        env is the various system information where this app is running
        """
        try:
            ra_git_version = (
                subprocess.run(
                    ["git", "rev-parse", "HEAD"],
                    capture_output=True,
                    check=True,
                    )
                .stdout.decode("utf-8")
                .strip()
            )
        except subprocess.CalledProcessError as error:
            logging.warning(ERROR_FORMAT, error.cmd, error.stderr.decode("utf-8"))
            ra_git_version = ""

        try:
            pi_version = (
                subprocess.run(
                    ["uname", "-a"],
                    capture_output=True,
                    check=True,
                    )
                .stdout.decode("utf-8")
                .strip()
            )
        except subprocess.CalledProcessError as error:
            logging.warning(ERROR_FORMAT, " ".join(error.cmd), error.stderr.decode("utf-8"))
            pi_version = "?"

        return {"git": ra_git_version, "env": pi_version}

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
            logging.warning(ERROR_FORMAT, error.cmd, error.stderr.decode("utf-8"))
            processes = ""

        matching_processes = findall(daemon, processes)
        return len(matching_processes)

    @staticmethod
    def is_bridge_setup():
        """
        Returns (bool) True if the rascsi_bridge network interface exists
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
            logging.warning(ERROR_FORMAT, error.cmd, error.stderr.decode("utf-8"))
            bridges = ""

        if "rascsi_bridge" in bridges:
            return True
        return False

    @staticmethod
    def disk_space():
        """
        Returns a (dict) with (int) total (int) used (int) free
        This is the disk space information of the volume where this app is running
        """
        total, used, free = disk_usage(__file__)
        return {"total": total, "used": used, "free": free}

    @staticmethod
    def introspect_file(file_path, re_term):
        """
        Takes a (str) file_path and (str) re_term in regex format
        Will introspect file_path for the existance of re_term
        and return True if found, False if not found
        """
        try:
            ifile = open(file_path, "r", encoding="ISO-8859-1")
        except (IOError, ValueError, EOFError, TypeError) as error:
            logging.error(str(error))
            return False
        for line in ifile:
            if match(re_term, line):
                return True
        return False

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
            sock.connect(('10.255.255.255', 1))
            ip_addr = sock.getsockname()[0]
        except Exception:
            ip_addr = False
        finally:
            sock.close()
        return ip_addr, host

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
