"""
Module for the Flask app rendering and endpoints
"""

import logging
import argparse
from pathlib import Path
from functools import wraps
from hashlib import md5

from flask import (
    Flask,
    render_template,
    request,
    flash,
    url_for,
    redirect,
    send_file,
    send_from_directory,
    make_response,
    session,
    abort,
)
from flask_babel import Babel, Locale, refresh, _

from pi_cmds import (
    running_env,
    running_proc,
    is_bridge_setup,
    disk_space,
    get_ip_address,
    introspect_file,
    auth_active,
)

from device_utils import (
    sort_and_format_devices,
    get_valid_scsi_ids,
)
from return_code_mapper import ReturnCodeMapper

from settings import (
    AFP_DIR,
    MAX_FILE_SIZE,
    ARCHIVE_FILE_SUFFIX,
    DEFAULT_CONFIG,
    DRIVE_PROPERTIES_FILE,
    AUTH_GROUP,
    LANGUAGES,
)

from rascsi.common_settings import (
    CFG_DIR,
    CONFIG_FILE_SUFFIX,
    PROPERTIES_SUFFIX,
    REMOVABLE_DEVICE_TYPES,
    RESERVATIONS,
)
from rascsi.ractl_cmds import RaCtlCmds
from rascsi.file_cmds import FileCmds

from socket_cmds_flask import SocketCmdsFlask


APP = Flask(__name__)
BABEL = Babel(APP)


@BABEL.localeselector
def get_locale():
    """
    Uses the session language, or tries to detect based on accept-languages header
    """
    try:
        language = session["language"]
    except KeyError:
        language = ""
        logging.warning("The default locale could not be detected. Falling back to English.")
    if language:
        return language
    # Hardcoded fallback to "en" when the user agent does not send an accept-language header
    language = request.accept_languages.best_match(LANGUAGES) or "en"
    return language


def get_supported_locales():
    """
    Returns a list of Locale objects that the Web Interfaces supports
    """
    locales = BABEL.list_translations()
    locales.append(Locale("en"))
    sorted_locales = sorted(locales, key=lambda x: x.language)
    return sorted_locales


@APP.route("/")
def index():
    """
    Sets up data structures for and renders the index page
    """
    if not ractl.is_token_auth()["status"] and not APP.config["TOKEN"]:
        abort(
            403,
            _(
                "RaSCSI is password protected. "
                "Start the Web Interface with the --password parameter."
                ),
            )

    locales = get_supported_locales()
    server_info = ractl.get_server_info()
    disk = disk_space()
    devices = ractl.list_devices()
    device_types = ractl.get_device_types()
    image_files = file_cmds.list_images()
    config_files = file_cmds.list_config_files()

    sorted_image_files = sorted(image_files["files"], key=lambda x: x["name"].lower())
    sorted_config_files = sorted(config_files, key=lambda x: x.lower())

    attached_images = []
    units = 0
    # If there are more than 0 logical unit numbers, display in the Web UI
    for device in devices["device_list"]:
        attached_images.append(device["image"].replace(server_info["image_dir"] + "/", ""))
        units += int(device["unit"])

    reserved_scsi_ids = server_info["reserved_ids"]
    scsi_ids, recommended_id = get_valid_scsi_ids(devices["device_list"], reserved_scsi_ids)
    formatted_devices = sort_and_format_devices(devices["device_list"])

    valid_file_suffix = "."+", .".join(
        server_info["sahd"] +
        server_info["schd"] +
        server_info["scrm"] +
        server_info["scmo"] +
        server_info["sccd"] +
        [ARCHIVE_FILE_SUFFIX]
        )

    if "username" in session:
        username = session["username"]
    else:
        username = None

    return render_template(
        "index.html",
        locales=locales,
        bridge_configured=is_bridge_setup(),
        netatalk_configured=running_proc("afpd"),
        macproxy_configured=running_proc("macproxy"),
        ip_addr=get_ip_address(),
        devices=formatted_devices,
        files=sorted_image_files,
        config_files=sorted_config_files,
        base_dir=server_info["image_dir"],
        scan_depth=server_info["scan_depth"],
        CFG_DIR=CFG_DIR,
        AFP_DIR=AFP_DIR,
        scsi_ids=scsi_ids,
        recommended_id=recommended_id,
        attached_images=attached_images,
        units=units,
        reserved_scsi_ids=reserved_scsi_ids,
        RESERVATIONS=RESERVATIONS,
        max_file_size=int(int(MAX_FILE_SIZE) / 1024 / 1024),
        running_env=running_env(),
        version=server_info["version"],
        log_levels=server_info["log_levels"],
        current_log_level=server_info["current_log_level"],
        netinfo=ractl.get_network_info(),
        device_types=device_types["device_types"],
        free_disk=int(disk["free"] / 1024 / 1024),
        valid_file_suffix=valid_file_suffix,
        cdrom_file_suffix=tuple(server_info["sccd"]),
        removable_file_suffix=tuple(server_info["scrm"]),
        mo_file_suffix=tuple(server_info["scmo"]),
        username=username,
        auth_active=auth_active()["status"],
        ARCHIVE_FILE_SUFFIX=ARCHIVE_FILE_SUFFIX,
        PROPERTIES_SUFFIX=PROPERTIES_SUFFIX,
        REMOVABLE_DEVICE_TYPES=REMOVABLE_DEVICE_TYPES,
    )


@APP.route("/drive/list", methods=["GET"])
def drive_list():
    """
    Sets up the data structures and kicks off the rendering of the drive list page
    """
    server_info = ractl.get_server_info()
    disk = disk_space()

    # Reads the canonical drive properties into a dict
    # The file resides in the current dir of the web ui process
    drive_properties = Path(DRIVE_PROPERTIES_FILE)
    if drive_properties.is_file():
        process = file_cmds.read_drive_properties(str(drive_properties))
        process = ReturnCodeMapper.add_msg(process)
        if not process["status"]:
            flash(process["msg"], "error")
            return redirect(url_for("index"))
        conf = process["conf"]
    else:
        flash(
                _(
                    "Could not read drive properties from %(properties_file)s",
                    properties_file=drive_properties,
                    ),
                "error",
                )
        return redirect(url_for("index"))

    hd_conf = []
    cd_conf = []
    rm_conf = []

    from werkzeug.utils import secure_filename
    for device in conf:
        if device["device_type"] == "SCHD":
            device["secure_name"] = secure_filename(device["name"])
            device["size_mb"] = "{:,.2f}".format(device["size"] / 1024 / 1024)
            hd_conf.append(device)
        elif device["device_type"] == "SCCD":
            device["size_mb"] = "N/A"
            cd_conf.append(device)
        elif device["device_type"] == "SCRM":
            device["secure_name"] = secure_filename(device["name"])
            device["size_mb"] = "{:,.2f}".format(device["size"] / 1024 / 1024)
            rm_conf.append(device)

    files = file_cmds.list_images()
    sorted_image_files = sorted(files["files"], key=lambda x: x["name"].lower())
    hd_conf = sorted(hd_conf, key=lambda x: x["name"].lower())
    cd_conf = sorted(cd_conf, key=lambda x: x["name"].lower())
    rm_conf = sorted(rm_conf, key=lambda x: x["name"].lower())

    if "username" in session:
        username = session["username"]
    else:
        username = None

    return render_template(
        "drives.html",
        files=sorted_image_files,
        base_dir=server_info["image_dir"],
        hd_conf=hd_conf,
        cd_conf=cd_conf,
        rm_conf=rm_conf,
        running_env=running_env(),
        version=server_info["version"],
        free_disk=int(disk["free"] / 1024 / 1024),
        cdrom_file_suffix=tuple(server_info["sccd"]),
        username=username,
        auth_active=auth_active()["status"],
    )


@APP.route("/login", methods=["POST"])
def login():
    """
    Uses simplepam to authenticate against Linux users
    """
    username = request.form["username"]
    password = request.form["password"]

    from simplepam import authenticate
    from grp import getgrall

    groups = [g.gr_name for g in getgrall() if username in g.gr_mem]
    if AUTH_GROUP in groups:
        if authenticate(str(username), str(password)):
            session["username"] = request.form["username"]
            return redirect(url_for("index"))
    flash(
            _(
                "You must log in with credentials for a user in the '%(group)s' group",
                group=AUTH_GROUP,
                ),
            "error",
            )
    return redirect(url_for("index"))


@APP.route("/logout")
def logout():
    """
    Removes the logged in user from the session
    """
    session.pop("username", None)
    return redirect(url_for("index"))


@APP.route("/pwa/<path:path>")
def send_pwa_files(path):
    """
    Sets up mobile web resources
    """
    return send_from_directory("pwa", path)


def login_required(func):
    """
    Wrapper method for enabling authentication for an endpoint
    """
    @wraps(func)
    def decorated_function(*args, **kwargs):
        auth = auth_active()
        if auth["status"] and "username" not in session:
            flash(auth["msg"], "error")
            return redirect(url_for("index"))
        return func(*args, **kwargs)
    return decorated_function


@APP.route("/drive/create", methods=["POST"])
@login_required
def drive_create():
    """
    Creates the image and properties file pair
    """
    vendor = request.form.get("vendor")
    product = request.form.get("product")
    revision = request.form.get("revision")
    block_size = request.form.get("block_size")
    size = request.form.get("size")
    file_type = request.form.get("file_type")
    file_name = request.form.get("file_name")
    full_file_name = file_name + "." + file_type

    # Creating the image file
    process = file_cmds.create_new_image(file_name, file_type, size)
    if process["status"]:
        flash(_("Image file created: %(file_name)s", file_name=full_file_name))
    else:
        flash(process["msg"], "error")
        return redirect(url_for("index"))

    # Creating the drive properties file
    prop_file_name = f"{file_name}.{file_type}.{PROPERTIES_SUFFIX}"
    properties = {
                "vendor": vendor,
                "product": product,
                "revision": revision,
                "block_size": block_size,
                }
    process = file_cmds.write_drive_properties(prop_file_name, properties)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(process['msg'], "error")
    return redirect(url_for("index"))


@APP.route("/drive/cdrom", methods=["POST"])
@login_required
def drive_cdrom():
    """
    Creates a properties file for a CD-ROM image
    """
    vendor = request.form.get("vendor")
    product = request.form.get("product")
    revision = request.form.get("revision")
    block_size = request.form.get("block_size")
    file_name = request.form.get("file_name")

    # Creating the drive properties file
    file_name = f"{file_name}.{PROPERTIES_SUFFIX}"
    properties = {
        "vendor": vendor,
        "product": product,
        "revision": revision,
        "block_size": block_size,
        }
    process = file_cmds.write_drive_properties(file_name, properties)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(process['msg'], "error")
    return redirect(url_for("index"))


@APP.route("/config/save", methods=["POST"])
@login_required
def config_save():
    """
    Saves a config file to disk
    """
    file_name = request.form.get("name") or "default"
    file_name = f"{file_name}.{CONFIG_FILE_SUFFIX}"

    process = file_cmds.write_config(file_name)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(process['msg'], "error")
    return redirect(url_for("index"))


@APP.route("/config/load", methods=["POST"])
@login_required
def config_load():
    """
    Loads a config file from disk
    """
    file_name = request.form.get("name")

    if "load" in request.form:
        process = file_cmds.read_config(file_name)
        process = ReturnCodeMapper.add_msg(process)
        if process["status"]:
            flash(process["msg"])
            return redirect(url_for("index"))

        flash(process['msg'], "error")
        return redirect(url_for("index"))
    if "delete" in request.form:
        process = file_cmds.delete_file(f"{CFG_DIR}/{file_name}")
        process = ReturnCodeMapper.add_msg(process)
        if process["status"]:
            flash(process["msg"])
            return redirect(url_for("index"))

        flash(process['msg'], "error")
        return redirect(url_for("index"))

    # The only reason we would reach here would be a Web UI bug. Will not localize.
    flash("Got an unhandled request (needs to be either load or delete)", "error")
    return redirect(url_for("index"))


@APP.route("/logs/show", methods=["POST"])
def show_logs():
    """
    Displays system logs
    """
    lines = request.form.get("lines") or "200"
    scope = request.form.get("scope") or "default"

    from subprocess import run
    if scope != "default":
        process = run(
                ["journalctl", "-n", lines, "-u", scope],
                capture_output=True,
                check=True,
                )
    else:
        process = run(
                ["journalctl", "-n", lines],
                capture_output=True,
                check=True,
                )

    if process.returncode == 0:
        headers = {"content-type": "text/plain"}
        return process.stdout.decode("utf-8"), int(lines), headers

    flash(_("An error occurred when fetching logs."))
    flash(process.stderr.decode("utf-8"), "stderr")
    return redirect(url_for("index"))


@APP.route("/logs/level", methods=["POST"])
@login_required
def log_level():
    """
    Sets RaSCSI backend log level
    """
    level = request.form.get("level") or "info"

    process = ractl.set_log_level(level)
    if process["status"]:
        flash(_("Log level set to %(value)s", value=level))
        return redirect(url_for("index"))

    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/daynaport/attach", methods=["POST"])
@login_required
def daynaport_attach():
    """
    Attaches a DaynaPORT ethernet adapter device
    """
    scsi_id = request.form.get("scsi_id")
    interface = request.form.get("if")
    ip_addr = request.form.get("ip")
    mask = request.form.get("mask")

    error_url = "https://github.com/akuker/RASCSI/wiki/Dayna-Port-SCSI-Link"
    error_msg = _("Please follow the instructions at %(url)s", url=error_url)

    if interface.startswith("wlan"):
        if not introspect_file("/etc/sysctl.conf", r"^net\.ipv4\.ip_forward=1$"):
            flash(_("Configure IPv4 forwarding before using a wireless network device."), "error")
            flash(error_msg, "error")
            return redirect(url_for("index"))
        if not Path("/etc/iptables/rules.v4").is_file():
            flash(_("Configure NAT before using a wireless network device."), "error")
            flash(error_msg, "error")
            return redirect(url_for("index"))
    else:
        if not introspect_file("/etc/dhcpcd.conf", r"^denyinterfaces " + interface + r"$"):
            flash(_("Configure the network bridge before using a wired network device."), "error")
            flash(error_msg, "error")
            return redirect(url_for("index"))
        if not Path("/etc/network/interfaces.d/rascsi_bridge").is_file():
            flash(_("Configure the network bridge before using a wired network device."), "error")
            flash(error_msg, "error")
            return redirect(url_for("index"))

    kwargs = {"device_type": "SCDP"}
    if interface != "":
        arg = interface
        if "" not in (ip_addr, mask):
            arg += (":" + ip_addr + "/" + mask)
        kwargs["interfaces"] = arg

    process = ractl.attach_image(scsi_id, **kwargs)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        flash(_("Attached DaynaPORT to SCSI ID %(id_number)s", id_number=scsi_id))
        return redirect(url_for("index"))

    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/scsi/attach", methods=["POST"])
@login_required
def attach():
    """
    Attaches a file image as a device
    """
    file_name = request.form.get("file_name")
    file_size = request.form.get("file_size")
    scsi_id = request.form.get("scsi_id")
    unit = request.form.get("unit")
    device_type = request.form.get("type")

    kwargs = {"unit": int(unit), "image": file_name}

    # The most common block size is 512 bytes
    expected_block_size = 512

    if device_type != "":
        kwargs["device_type"] = device_type
        if device_type == "SCCD":
            expected_block_size = 2048
        elif device_type == "SAHD":
            expected_block_size = 256

    # Attempt to load the device properties file:
    # same file name with PROPERTIES_SUFFIX appended
    drive_properties = f"{CFG_DIR}/{file_name}.{PROPERTIES_SUFFIX}"
    if Path(drive_properties).is_file():
        process = file_cmds.read_drive_properties(drive_properties)
        process = ReturnCodeMapper.add_msg(process)
        if not process["status"]:
            flash(process["msg"], "error")
            return redirect(url_for("index"))
        conf = process["conf"]
        kwargs["vendor"] = conf["vendor"]
        kwargs["product"] = conf["product"]
        kwargs["revision"] = conf["revision"]
        kwargs["block_size"] = conf["block_size"]
        expected_block_size = conf["block_size"]

    process = ractl.attach_image(scsi_id, **kwargs)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        flash(_("Attached %(file_name)s to SCSI ID %(id_number)s LUN %(unit_number)s",
            file_name=file_name, id_number=scsi_id, unit_number=unit))
        if int(file_size) % int(expected_block_size):
            flash(_("The image file size %(file_size)s bytes is not a multiple of "
                  u"%(block_size)s. RaSCSI will ignore the trailing data. "
                  u"The image may be corrupted, so proceed with caution.",
                  file_size=file_size, block_size=expected_block_size), "error")
        return redirect(url_for("index"))

    flash(_("Failed to attach %(file_name)s to SCSI ID %(id_number)s LUN %(unit_number)s",
        file_name=file_name, id_number=scsi_id, unit_number=unit), "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/scsi/detach_all", methods=["POST"])
@login_required
def detach_all_devices():
    """
    Detaches all currently attached devices
    """
    process = ractl.detach_all()
    if process["status"]:
        flash(_("Detached all SCSI devices"))
        return redirect(url_for("index"))

    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/scsi/detach", methods=["POST"])
@login_required
def detach():
    """
    Detaches a specified device
    """
    scsi_id = request.form.get("scsi_id")
    unit = request.form.get("unit")
    process = ractl.detach_by_id(scsi_id, unit)
    if process["status"]:
        flash(_("Detached SCSI ID %(id_number)s LUN %(unit_number)s",
            id_number=scsi_id, unit_number=unit))
        return redirect(url_for("index"))

    flash(_("Failed to detach SCSI ID %(id_number)s LUN %(unit_number)s",
        id_number=scsi_id, unit_number=unit), "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/scsi/eject", methods=["POST"])
@login_required
def eject():
    """
    Ejects a specified removable device image, but keeps the device attached
    """
    scsi_id = request.form.get("scsi_id")
    unit = request.form.get("unit")

    process = ractl.eject_by_id(scsi_id, unit)
    if process["status"]:
        flash(_("Ejected SCSI ID %(id_number)s LUN %(unit_number)s",
            id_number=scsi_id, unit_number=unit))
        return redirect(url_for("index"))

    flash(_("Failed to eject SCSI ID %(id_number)s LUN %(unit_number)s",
        id_number=scsi_id, unit_number=unit), "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))

@APP.route("/scsi/info", methods=["POST"])
def device_info():
    """
    Displays detailed info for a specific device
    """
    scsi_id = request.form.get("scsi_id")
    unit = request.form.get("unit")

    devices = ractl.list_devices(scsi_id, unit)

    # First check if any device at all was returned
    if not devices["status"]:
        flash(devices["msg"], "error")
        return redirect(url_for("index"))
    # Looking at the first dict in list to get
    # the one and only device that should have been returned
    device = devices["device_list"][0]
    if str(device["id"]) == scsi_id:
        flash(_("DEVICE INFO"))
        flash("===========")
        flash(_("SCSI ID: %(id_number)s", id_number=device["id"]))
        flash(_("LUN: %(unit_number)s", unit_number=device["unit"]))
        flash(_("Type: %(device_type)s", device_type=device["device_type"]))
        flash(_("Status: %(device_status)s", device_status=device["status"]))
        flash(_("File: %(image_file)s", image_file=device["image"]))
        flash(_("Parameters: %(value)s", value=device["params"]))
        flash(_("Vendor: %(value)s", value=device["vendor"]))
        flash(_("Product: %(value)s", value=device["product"]))
        flash(_("Revision: %(revision_number)s", revision_number=device["revision"]))
        flash(_("Block Size: %(value)s bytes", value=device["block_size"]))
        flash(_("Image Size: %(value)s bytes", value=device["size"]))
        return redirect(url_for("index"))

    flash(devices["msg"], "error")
    return redirect(url_for("index"))

@APP.route("/scsi/reserve", methods=["POST"])
@login_required
def reserve_id():
    """
    Reserves a SCSI ID and stores the memo for that reservation
    """
    scsi_id = request.form.get("scsi_id")
    memo = request.form.get("memo")
    reserved_ids = ractl.get_reserved_ids()["ids"]
    reserved_ids.extend(scsi_id)
    process = ractl.reserve_scsi_ids(reserved_ids)
    if process["status"]:
        RESERVATIONS[int(scsi_id)] = memo
        flash(_("Reserved SCSI ID %(id_number)s", id_number=scsi_id))
        return redirect(url_for("index"))

    flash(_("Failed to reserve SCSI ID %(id_number)s", id_number=scsi_id))
    flash(process["msg"], "error")
    return redirect(url_for("index"))

@APP.route("/scsi/unreserve", methods=["POST"])
@login_required
def unreserve_id():
    """
    Removes the reservation of a SCSI ID as well as the memo for the reservation
    """
    scsi_id = request.form.get("scsi_id")
    reserved_ids = ractl.get_reserved_ids()["ids"]
    reserved_ids.remove(scsi_id)
    process = ractl.reserve_scsi_ids(reserved_ids)
    if process["status"]:
        RESERVATIONS[int(scsi_id)] = ""
        flash(_("Released the reservation for SCSI ID %(id_number)s", id_number=scsi_id))
        return redirect(url_for("index"))

    flash(_("Failed to release the reservation for SCSI ID %(id_number)s", id_number=scsi_id))
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/pi/reboot", methods=["POST"])
@login_required
def restart():
    """
    Restarts the Pi
    """
    ractl.shutdown_pi("reboot")
    return redirect(url_for("index"))


@APP.route("/pi/shutdown", methods=["POST"])
@login_required
def shutdown():
    """
    Shuts down the Pi
    """
    ractl.shutdown_pi("system")
    return redirect(url_for("index"))


@APP.route("/files/download_to_iso", methods=["POST"])
@login_required
def download_to_iso():
    """
    Downloads a remote file and creates a CD-ROM image formatted with HFS that contains the file
    """
    scsi_id = request.form.get("scsi_id")
    url = request.form.get("url")
    iso_args = request.form.get("type").split()

    process = file_cmds.download_file_to_iso(url, *iso_args)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        flash(process["msg"])
        flash(_("Saved image as: %(file_name)s", file_name=process['file_name']))
    else:
        flash(_("Failed to create CD-ROM image from %(url)s", url=url), "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))

    process_attach = ractl.attach_image(scsi_id, device_type="SCCD", image=process["file_name"])
    process_attach = ReturnCodeMapper.add_msg(process_attach)
    if process_attach["status"]:
        flash(_("Attached to SCSI ID %(id_number)s", id_number=scsi_id))
        return redirect(url_for("index"))

    flash(_("Failed to attach image to SCSI ID %(id_number)s. Try attaching it manually.",
        id_number=scsi_id), "error")
    flash(process_attach["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/files/download_to_images", methods=["POST"])
@login_required
def download_img():
    """
    Downloads a remote file onto the images dir on the Pi
    """
    url = request.form.get("url")
    server_info = ractl.get_server_info()
    process = file_cmds.download_to_dir(url, server_info["image_dir"], Path(url).name)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(_("Failed to download file from %(url)s", url=url), "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/files/download_to_afp", methods=["POST"])
@login_required
def download_afp():
    """
    Downloads a remote file onto the AFP shared dir on the Pi
    """
    url = request.form.get("url")
    file_name = Path(url).name
    # Shorten file names longer than 31 chars, the HFS upper limit
    # Append a hash of the removed piece of the file name to make it unique
    # The format of the hash is a # followed by the first four symbols of the md4 hash
    if len(file_name) > 31:
        chars_to_cut = len(file_name) - 31
        file_name_stem = Path(file_name).stem
        file_name_suffix = Path(file_name).suffix
        discarded_portion = file_name_stem[:-abs(chars_to_cut)]
        stem_remainder = file_name_stem[:(26 - len(file_name_suffix))]
        appendix_hash = (md5(discarded_portion.encode("utf-8"))).hexdigest().upper()
        file_name = stem_remainder + "#" + appendix_hash[:4] + file_name_suffix
    process = file_cmds.download_to_dir(url, AFP_DIR, file_name)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(_("Failed to download file from %(url)s", url=url), "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/files/upload", methods=["POST"])
def upload_file():
    """
    Uploads a file from the local computer to the images dir on the Pi
    Depending on the Dropzone.js JavaScript library
    """
    # Due to the embedded javascript library, we cannot use the @login_required decorator
    auth = auth_active()
    if auth["status"] and "username" not in session:
        return make_response(auth["msg"], 403)

    from werkzeug.utils import secure_filename
    from os import path

    log = logging.getLogger("pydrop")
    file_object = request.files["file"]
    file_name = secure_filename(file_object.filename)

    server_info = ractl.get_server_info()

    save_path = path.join(server_info["image_dir"], file_name)
    current_chunk = int(request.form['dzchunkindex'])

    # Makes sure not to overwrite an existing file,
    # but continues writing to a file transfer in progress
    if path.exists(save_path) and current_chunk == 0:
        return make_response(_("The file already exists!"), 400)

    try:
        with open(save_path, "ab") as save:
            save.seek(int(request.form["dzchunkbyteoffset"]))
            save.write(file_object.stream.read())
    except OSError:
        log.exception("Could not write to file")
        return make_response(_("Unable to write the file to disk!"), 500)

    total_chunks = int(request.form["dztotalchunkcount"])

    if current_chunk + 1 == total_chunks:
        # Validate the resulting file size after writing the last chunk
        if path.getsize(save_path) != int(request.form["dztotalfilesize"]):
            log.error(
                    "Finished transferring %s, "
                    "but it has a size mismatch with the original file. "
                    "Got %s but we expected %s.",
                    file_object.filename,
                    path.getsize(save_path),
                    request.form['dztotalfilesize'],
                    )
            return make_response(_("Transferred file corrupted!"), 500)

        log.info("File %s has been uploaded successfully", file_object.filename)
    log.debug("Chunk %s of %s for file %s completed.",
              current_chunk + 1, total_chunks, file_object.filename)

    return make_response(_("File upload successful!"), 200)


@APP.route("/files/create", methods=["POST"])
@login_required
def create_file():
    """
    Creates an empty image file in the images dir
    """
    file_name = request.form.get("file_name")
    size = (int(request.form.get("size")) * 1024 * 1024)
    file_type = request.form.get("type")
    full_file_name = file_name + "." + file_type

    process = file_cmds.create_new_image(file_name, file_type, size)
    if process["status"]:
        flash(_("Image file created: %(file_name)s", file_name=full_file_name))
        return redirect(url_for("index"))

    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/files/download", methods=["POST"])
@login_required
def download():
    """
    Downloads a file from the Pi to the local computer
    """
    image = request.form.get("file")
    return send_file(image, as_attachment=True)


@APP.route("/files/delete", methods=["POST"])
@login_required
def delete():
    """
    Deletes a specified file in the images dir
    """
    file_name = request.form.get("file_name")

    process = file_cmds.delete_image(file_name)
    if process["status"]:
        flash(_("Image file deleted: %(file_name)s", file_name=file_name))
    else:
        flash(process["msg"], "error")
        return redirect(url_for("index"))

    # Delete the drive properties file, if it exists
    prop_file_path = f"{CFG_DIR}/{file_name}.{PROPERTIES_SUFFIX}"
    if Path(prop_file_path).is_file():
        process = file_cmds.delete_file(prop_file_path)
        process = ReturnCodeMapper.add_msg(process)

        if process["status"]:
            flash(process["msg"])
            return redirect(url_for("index"))

        flash(process["msg"], "error")
        return redirect(url_for("index"))

    return redirect(url_for("index"))


@APP.route("/files/rename", methods=["POST"])
@login_required
def rename():
    """
    Renames a specified file in the images dir
    """
    file_name = request.form.get("file_name")
    new_file_name = request.form.get("new_file_name")

    process = file_cmds.rename_image(file_name, new_file_name)
    if process["status"]:
        flash(_("Image file renamed to: %(file_name)s", file_name=new_file_name))
    else:
        flash(process["msg"], "error")
        return redirect(url_for("index"))

    # Rename the drive properties file, if it exists
    prop_file_path = f"{CFG_DIR}/{file_name}.{PROPERTIES_SUFFIX}"
    new_prop_file_path = f"{CFG_DIR}/{new_file_name}.{PROPERTIES_SUFFIX}"
    if Path(prop_file_path).is_file():
        process = file_cmds.rename_file(prop_file_path, new_prop_file_path)
        process = ReturnCodeMapper.add_msg(process)
        if process["status"]:
            flash(process["msg"])
            return redirect(url_for("index"))

        flash(process["msg"], "error")
        return redirect(url_for("index"))

    return redirect(url_for("index"))


@APP.route("/files/unzip", methods=["POST"])
@login_required
def unzip():
    """
    Unzips all files in a specified zip archive, or a single file in the zip archive
    """
    zip_file = request.form.get("zip_file")
    zip_member = request.form.get("zip_member") or False
    zip_members = request.form.get("zip_members") or False

    from ast import literal_eval
    if zip_members:
        zip_members = literal_eval(zip_members)

    process = file_cmds.unzip_file(zip_file, zip_member, zip_members)
    if process["status"]:
        if not process["msg"]:
            flash(_("Aborted unzip: File(s) with the same name already exists."), "error")
            return redirect(url_for("index"))
        flash(_("Unzipped the following files:"))
        for msg in process["msg"]:
            flash(msg)
        if process["prop_flag"]:
            flash(_("Properties file(s) have been moved to %(directory)s", directory=CFG_DIR))
        return redirect(url_for("index"))

    flash(_("Failed to unzip %(zip_file)s", zip_file=zip_file), "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@APP.route("/language", methods=["POST"])
def change_language():
    """
    Changes the session language locale and refreshes the Flask app context
    """
    locale = request.form.get("locale")
    session["language"] = locale
    ractl.locale = session["language"]
    file_cmds.locale = session["language"]
    refresh()

    flash(_("Changed Web Interface language to %(locale)s", locale=locale))
    return redirect(url_for("index"))


@APP.before_first_request
def load_default_config():
    """
    Webapp initialization steps that require the Flask app to have started:
    - Get the detected locale to use for localizations
    - Load the default configuration file, if found
    """
    session["language"] = get_locale()
    ractl.locale = session["language"]
    file_cmds.locale = session["language"]
    if Path(f"{CFG_DIR}/{DEFAULT_CONFIG}").is_file():
        file_cmds.read_config(DEFAULT_CONFIG)


if __name__ == "__main__":
    APP.secret_key = "rascsi_is_awesome_insecure_secret_key"
    APP.config["SESSION_TYPE"] = "filesystem"
    APP.config["MAX_CONTENT_LENGTH"] = int(MAX_FILE_SIZE)

    parser = argparse.ArgumentParser(description="RaSCSI Web Interface command line arguments")
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        action="store",
        help="Port number the web server will run on",
        )
    parser.add_argument(
        "--password",
        type=str,
        default="",
        action="store",
        help="Token password string for authenticating with RaSCSI",
        )
    parser.add_argument(
        "--rascsi-host",
        type=str,
        default="localhost",
        action="store",
        help="RaSCSI host. Default: localhost",
    )
    parser.add_argument(
        "--rascsi-port",
        type=str,
        default=6868,
        action="store",
        help="RaSCSI port. Default: 6868",
    )
    arguments = parser.parse_args()
    APP.config["TOKEN"] = arguments.password

    sock_cmd = SocketCmdsFlask(host=arguments.rascsi_host, port=arguments.rascsi_port)
    ractl = RaCtlCmds(sock_cmd=sock_cmd, token=APP.config["TOKEN"])
    file_cmds = FileCmds(sock_cmd=sock_cmd, ractl=ractl, token=APP.config["TOKEN"])

    import bjoern
    print("Serving rascsi-web...")
    bjoern.run(APP, "0.0.0.0", arguments.port)
