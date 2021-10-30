import subprocess


def systemd_service(service, action):
    # start/stop/restart
    return (
        subprocess.run(["sudo", "/bin/systemctl", action, service]).returncode
        == 0
    )


def reboot_pi():
    subprocess.Popen(["sudo", "reboot"])
    return True


def shutdown_pi():
    subprocess.Popen(["sudo", "shutdown", "-h", "now"])
    return True


def running_env():
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


def running_netatalk():
    """
    Returns int afpd, which is the number of afpd processes currently running
    """
    process = subprocess.run(["ps", "aux"], capture_output=True)
    output = process.stdout.decode("utf-8")
    from re import findall
    afpd = findall("afpd", output)
    return len(afpd)


def is_bridge_setup():
    process = subprocess.run(["brctl", "show"], capture_output=True)
    output = process.stdout.decode("utf-8")
    if "rascsi_bridge" in output:
        return True
    return False


def disk_space():
    from shutil import disk_usage
    total, used, free = disk_usage(__file__)
    return {"total": total, "used": used, "free": free}
