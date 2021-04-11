import subprocess


def rascsi_service(action):
    # start/stop/restart
    return subprocess.run(["sudo", "/bin/systemctl", action, "rascsi.service"]).returncode == 0


def reboot_pi():
    return subprocess.run(["sudo", "reboot"]).returncode == 0


def shutdown_pi():
    return subprocess.run(["sudo", "shutdown", "-h", "now"]).returncode == 0


def running_version():
    ra_web_version = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True).stdout.decode("utf-8").strip()
    pi_version = subprocess.run(["uname", "-a"], capture_output=True).stdout.decode("utf-8").strip()
    return ra_web_version + " " + pi_version
