import subprocess


def rascsi_service(action):
    # start/stop/restart
    return (
        subprocess.run(["sudo", "/bin/systemctl", action, "rascsi.service"]).returncode
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


def is_bridge_setup():
    process = subprocess.run(["brctl", "show"], capture_output=True)
    output = process.stdout.decode("utf-8")
    if "rascsi_bridge" in output:
        return True
    return False


def net_interfaces():
    from psutil import net_if_addrs
    ifaddrs = net_if_addrs()
    # Eliminates the lo and rascsi_bridge interfaces from the list, if present
    ifaddrs.pop("lo")
    ifaddrs.pop("rascsi_bridge")
    ifs = list(ifaddrs.keys())
    return ifs
