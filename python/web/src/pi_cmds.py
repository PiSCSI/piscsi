"""
Module for methods controlling and getting information about the Pi's Linux system
"""

import subprocess
import logging
from flask_babel import _
from settings import AUTH_GROUP


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
        logging.warning("Executed shell command: %s", " ".join(error.cmd))
        logging.warning("Got error: %s", error.stderr.decode("utf-8"))
        processes = ""

    from re import findall
    matching_processes = findall(daemon, processes)
    return len(matching_processes)


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
        logging.warning("Executed shell command: %s", " ".join(error.cmd))
        logging.warning("Got error: %s", error.stderr.decode("utf-8"))
        bridges = ""

    if "rascsi_bridge" in bridges:
        return True
    return False


def disk_space():
    """
    Returns a (dict) with (int) total (int) used (int) free
    This is the disk space information of the volume where this app is running
    """
    from shutil import disk_usage
    total, used, free = disk_usage(__file__)
    return {"total": total, "used": used, "free": free}


def get_ip_address():
    """
    Use a mock socket connection to identify the Pi's IP address
    """
    from socket import socket, AF_INET, SOCK_DGRAM
    sock = socket(AF_INET, SOCK_DGRAM)
    try:
        # mock ip address; doesn't have to be reachable
        sock.connect(('10.255.255.255', 1))
        ip_addr = sock.getsockname()[0]
    except Exception:
        ip_addr = '127.0.0.1'
    finally:
        sock.close()
    return ip_addr


def introspect_file(file_path, re_term):
    """
    Takes a (str) file_path and (str) re_term in regex format
    Will introspect file_path for the existance of re_term
    and return True if found, False if not found
    """
    from re import match
    try:
        ifile = open(file_path, "r", encoding="ISO-8859-1")
    except:
        return False
    for line in ifile:
        if match(re_term, line):
            return True
    return False


def auth_active():
    """
    Inspects if the group defined in AUTH_GROUP exists on the system.
    If it exists, tell the webapp to enable authentication.
    Returns a (dict) with (bool) status and (str) msg
    """
    from grp import getgrall
    groups = [g.gr_name for g in getgrall()]
    if AUTH_GROUP in groups:
        return {
                "status": True,
                "msg": _("You must log in to use this function"),
                }
    return {"status": False, "msg": ""}
