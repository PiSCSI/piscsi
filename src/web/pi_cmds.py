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
