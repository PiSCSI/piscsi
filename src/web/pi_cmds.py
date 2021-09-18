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


def running_version():
    ra_web_version = (
        subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True)
        .stdout.decode("utf-8")
        .strip()
    )
    pi_version = (
        subprocess.run(["uname", "-a"], capture_output=True)
        .stdout.decode("utf-8")
        .strip()
    )
    return "RaSCSI git revision: " + ra_web_version[:7] + " - Environment: " + pi_version


def is_bridge_setup():
    process = subprocess.run(["brctl", "show"], capture_output=True)
    output = process.stdout.decode("utf-8")
    if "rascsi_bridge" in output:
        return True
    return False
