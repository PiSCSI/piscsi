"""
Module for methods controlling and getting information about the Pi's Linux system
"""

import subprocess


def systemd_service(service, action):
    """
    Takes (str) service and (str) action
    Action can be one of start/stop/restart
    """
    return (
        subprocess.run(["sudo", "/bin/systemctl", action, service]).returncode
        == 0
    )


def reboot_pi():
    """
    Reboots the Pi system
    """
    subprocess.Popen(["sudo", "reboot"])
    return True


def shutdown_pi():
    """
    Shuts down the Pi system
    """
    subprocess.Popen(["sudo", "shutdown", "-h", "now"])
    return True


def running_env():
    """
    Returns (str) git and (str) env
    git contains the git hash of the checked out code
    env is the various system information where this app is running
    """
    ra_git_version = (
        subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True)
        .stdout.decode("utf-8")
        .strip()
    )
    pi_version = (
        subprocess.run(["uname", "-a"], capture_output=True)
        .stdout.decode("utf-8")
        .strip()
    )
    return {"git": ra_git_version, "env": pi_version}


def running_proc(daemon):
    """
    Takes (str) daemon
    Returns (int) proc, which is the number of processes currently running
    """
    process = subprocess.run(["ps", "aux"], capture_output=True)
    output = process.stdout.decode("utf-8")
    from re import findall
    proc = findall(daemon, output)
    return len(proc)


def is_bridge_setup():
    """
    Returns (bool) True if the rascsi_bridge network interface exists
    """
    process = subprocess.run(["brctl", "show"], capture_output=True)
    output = process.stdout.decode("utf-8")
    if "rascsi_bridge" in output:
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
        ifile = open(file_path, "r")
    except:
        return False
    for line in ifile:
        if match(re_term, line):
            return True
    return False


def auth_active():
    """
    Inspects if the 'rascsi' group exists on the system.
    If it exists, tell the webapp to enable authentication.
    """
    from grp import getgrall
    groups = [g.gr_name for g in getgrall()]
    if "rascsi" in groups:
        return {
                "status": True,
                "msg": "You must log in to use this function!",
                }
    return {"status": False, "msg": ""}
