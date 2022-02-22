"""
Module with methods that interact with the Pi system
"""
import subprocess
import logging
from flask_babel import _
from shutil import disk_usage
from re import findall
from socket import socket, gethostname, AF_INET, SOCK_DGRAM


class SysCmds:
    """
    Class for commands sent to the Pi's Linux system.
    """

    def running_env(self):
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
            logging.warning("Executed shell command: %s", " ".join(error.cmd))
            logging.warning("Got error: %s", error.stderr.decode("utf-8"))
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
            logging.warning("Executed shell command: %s", " ".join(error.cmd))
            logging.warning("Got error: %s", error.stderr.decode("utf-8"))
            pi_version = "Unknown"

        return {"git": ra_git_version, "env": pi_version}

    def running_proc(self, daemon):
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
            logging.warning("Executed shell command: %s", " ".join(error.cmd))
            logging.warning("Got error: %s", error.stderr.decode("utf-8"))
            processes = ""

        matching_processes = findall(daemon, processes)
        return len(matching_processes)

    def is_bridge_setup(self):
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
            logging.warning("Executed shell command: %s", " ".join(error.cmd))
            logging.warning("Got error: %s", error.stderr.decode("utf-8"))
            bridges = ""

        if "rascsi_bridge" in bridges:
            return True
        return False

    def disk_space(self):
        """
        Returns a (dict) with (int) total (int) used (int) free
        This is the disk space information of the volume where this app is running
        """
        total, used, free = disk_usage(__file__)
        return {"total": total, "used": used, "free": free}

    def introspect_file(self, file_path, re_term):
        """
        Takes a (str) file_path and (str) re_term in regex format
        Will introspect file_path for the existance of re_term
        and return True if found, False if not found
        """
        try:
            ifile = open(file_path, "r", encoding="ISO-8859-1")
        except:
            return False
        for line in ifile:
            if match(re_term, line):
                return True
        return False

    def get_ip_and_host(self):
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
