"""
Module for the Flask app rendering and endpoints
"""

import logging
import logging.config
import argparse
from pathlib import Path, PurePath
from functools import wraps
from grp import getgrall
from os import path
import bjoern

from piscsi.return_codes import ReturnCodes
from simplepam import authenticate
from flask_babel import Babel, Locale, refresh, _
from werkzeug.utils import secure_filename

from flask import (
    Flask,
    render_template,
    request,
    flash,
    url_for,
    redirect,
    send_from_directory,
    make_response,
    session,
    jsonify,
    abort,
)

from piscsi.piscsi_cmds import PiscsiCmds
from piscsi.file_cmds import FileCmds
from piscsi.sys_cmds import SysCmds

from piscsi.common_settings import (
    CFG_DIR,
    CONFIG_FILE_SUFFIX,
    PROPERTIES_SUFFIX,
    ARCHIVE_FILE_SUFFIXES,
    RESERVATIONS,
)

from return_code_mapper import ReturnCodeMapper
from socket_cmds_flask import SocketCmdsFlask

from web_utils import (
    working_dirs_exist,
    sort_and_format_devices,
    get_valid_scsi_ids,
    map_device_types_and_names,
    get_device_name,
    map_image_file_descriptions,
    format_image_list,
    format_drive_properties,
    get_properties_by_drive_name,
    auth_active,
    is_bridge_configured,
    is_safe_path,
    browser_supports_modern_themes,
)
from settings import (
    WEB_DIR,
    FILE_SERVER_DIR,
    MAX_FILE_SIZE,
    DEFAULT_CONFIG,
    DRIVE_PROPERTIES_FILE,
    AUTH_GROUP,
    LANGUAGES,
    TEMPLATE_THEMES,
    TEMPLATE_THEME_DEFAULT,
    TEMPLATE_THEME_LEGACY,
    THROTTLE_NOTIFY_MODES,
    THROTTLE_TEST_MODES,
)


def get_locale():
    """
    Uses the session language, or tries to detect based on accept-languages header
    """
    session_locale = session.get("language")
    if session_locale:
        return session_locale

    client_locale = request.accept_languages.best_match(LANGUAGES)
    if client_locale:
        return client_locale

    logging.info("The default locale could not be detected. Falling back to English.")
    return "en"


APP = Flask(__name__)
BABEL = Babel(APP)
BABEL.init_app(APP, locale_selector=get_locale)


def get_env_info():
    """
    Get information about the app/host environment
    """
    server_info = piscsi_cmd.get_server_info()
    ip_addr, host = sys_cmd.get_ip_and_host()

    if "username" in session:
        username = session["username"]
    else:
        username = None

    throttled_statuses = sys_cmd.get_throttled(THROTTLE_NOTIFY_MODES, THROTTLE_TEST_MODES)

    return {
        "running_env": sys_cmd.running_env(),
        "username": username,
        "auth_active": auth_active(AUTH_GROUP)["status"],
        "logged_in": username and auth_active(AUTH_GROUP)["status"],
        "ip_addr": ip_addr,
        "host": host,
        "system_name": sys_cmd.get_pretty_host(),
        "free_disk_space": int(sys_cmd.disk_space(server_info["image_dir"])["free"] / 1024 / 1024),
        "locale": get_locale(),
        "version": server_info["version"],
        "image_dir": server_info["image_dir"],
        "image_root_dir": Path(server_info["image_dir"]).name,
        "shared_root_dir": Path(FILE_SERVER_DIR).name,
        "cd_suffixes": tuple(server_info["sccd"]),
        "rm_suffixes": tuple(server_info["scrm"]),
        "mo_suffixes": tuple(server_info["scmo"]),
        "throttle_status": [
            (s[0], ReturnCodeMapper.add_msg({"return_code": s[1]})) for s in throttled_statuses
        ],
    }


def response(
    template=None,
    message=None,
    redirect_url=None,
    error=False,
    status_code=200,
    **kwargs,
):
    """
    Generates a HTML or JSON HTTP response
    """
    status = "error" if error else "success"

    if isinstance(message, list):
        messages = message
    elif message is None:
        messages = []
    else:
        messages = [(str(message), status)]

    if request.headers.get("accept") == "application/json":
        return (
            jsonify(
                {
                    "status": status,
                    "messages": [{"message": m, "category": c} for m, c in messages],
                    "data": kwargs,
                }
            ),
            status_code,
        )

    if messages:
        for message, category in messages:
            flash(message, category)

    if template:
        if session.get("theme") and session["theme"] in TEMPLATE_THEMES:
            theme = session["theme"]
        elif browser_supports_modern_themes():
            theme = TEMPLATE_THEME_DEFAULT
        else:
            theme = TEMPLATE_THEME_LEGACY

        body_classes = [f"page-{PurePath(template).stem.lower()}"]

        env_info = get_env_info()
        if env_info["auth_active"]:
            body_classes.append("auth-enabled")
            if env_info["logged_in"]:
                body_classes.append("logged-in")
        else:
            body_classes.append("auth-disabled")

        kwargs["env"] = env_info
        kwargs["body_classes"] = body_classes
        kwargs["current_theme_stylesheet"] = f"themes/{theme}/style.css"
        kwargs["current_theme"] = theme
        kwargs["available_themes"] = TEMPLATE_THEMES
        return render_template(template, **kwargs)

    if redirect_url:
        return redirect(url_for(redirect_url))
    return redirect(url_for("index"))


def get_supported_locales():
    """
    Returns a list of languages supported by the web UI
    """
    locales = [
        {"language": x.language, "display_name": x.display_name}
        for x in [*BABEL.list_translations()]
    ]

    return sorted(locales, key=lambda x: x["language"])


# pylint: disable=too-many-locals
@APP.route("/")
def index():
    """
    Sets up data structures for and renders the index page
    """
    server_info = piscsi_cmd.get_server_info()
    working_dirs_exist((server_info["image_dir"], CFG_DIR))

    devices = piscsi_cmd.list_devices()
    device_types = map_device_types_and_names(piscsi_cmd.get_device_types()["device_types"])
    image_files = piscsi_cmd.list_images()
    config_files = file_cmd.list_config_files()
    formatted_image_files = format_image_list(
        image_files["files"], Path(server_info["image_dir"]).name, device_types
    )

    attached_images = []
    units = 0
    # If there are more than 0 logical unit numbers, display in the Web UI
    for device in devices["device_list"]:
        attached_images.append(device["image"].replace(server_info["image_dir"] + "/", ""))
        units += int(device["unit"])

    reserved_scsi_ids = server_info["reserved_ids"]
    scsi_ids = get_valid_scsi_ids(devices["device_list"], reserved_scsi_ids)
    formatted_devices = sort_and_format_devices(devices["device_list"])

    image_suffixes_to_create = map_image_file_descriptions(
        # Here we strip out hdi and nhd, since they are proprietary PC-98 emulator formats
        # that require a particular header to work. We can't generate them on the fly.
        # We also reverse the resulting list, in order for hds to be the default in the UI.
        # This might break if something like 'hdt' etc. gets added in the future.
        sorted(
            [suffix for suffix in server_info["schd"] if suffix not in {"hdi", "nhd"}],
            reverse=True,
        )
        + server_info["scrm"]
        + server_info["scmo"]
    )

    valid_image_suffixes = (
        server_info["schd"] + server_info["scrm"] + server_info["scmo"] + server_info["sccd"]
    )

    return response(
        template="index.html",
        page_title=_("PiSCSI Control Page"),
        is_root_page=True,
        netinfo=piscsi_cmd.get_network_info(),
        bridge_configured=sys_cmd.is_bridge_setup(),
        devices=formatted_devices,
        attached_images=attached_images,
        formatted_image_files=formatted_image_files,
        files=image_files["files"],
        config_files=config_files,
        device_types=device_types,
        scan_depth=server_info["scan_depth"],
        scsi_ids=scsi_ids,
        units=units,
        reserved_scsi_ids=reserved_scsi_ids,
        image_suffixes_to_create=image_suffixes_to_create,
        valid_image_suffixes=valid_image_suffixes,
        drive_properties=format_drive_properties(APP.config["PISCSI_DRIVE_PROPERTIES"]),
        images_subdirs=file_cmd.list_subdirs(server_info["image_dir"]),
        shared_subdirs=file_cmd.list_subdirs(FILE_SERVER_DIR),
        file_server_dir_exists=Path(FILE_SERVER_DIR).exists(),
        RESERVATIONS=RESERVATIONS,
        CFG_DIR=CFG_DIR,
        FILE_SERVER_DIR=FILE_SERVER_DIR,
        PROPERTIES_SUFFIX=PROPERTIES_SUFFIX,
        ARCHIVE_FILE_SUFFIXES=ARCHIVE_FILE_SUFFIXES,
        CONFIG_FILE_SUFFIX=CONFIG_FILE_SUFFIX,
        REMOVABLE_DEVICE_TYPES=piscsi_cmd.get_removable_device_types(),
        DISK_DEVICE_TYPES=piscsi_cmd.get_disk_device_types(),
        PERIPHERAL_DEVICE_TYPES=piscsi_cmd.get_peripheral_device_types(),
    )


@APP.route("/env")
def env():
    """
    Shows information about the app/host environment
    """
    return response(**get_env_info())


@APP.route("/drive/list", methods=["GET"])
def drive_list():
    """
    Sets up the data structures and kicks off the rendering of the drive list page
    """
    server_info = piscsi_cmd.get_server_info()
    working_dirs_exist((server_info["image_dir"], CFG_DIR))

    return response(
        template="drives.html",
        page_title=_("PiSCSI Create Drive"),
        files=piscsi_cmd.list_images()["files"],
        drive_properties=format_drive_properties(APP.config["PISCSI_DRIVE_PROPERTIES"]),
    )


@APP.route("/sys/admin", methods=["GET"])
def admin():
    """
    Sets up the data structures and kicks off the rendering of the system admin page
    """
    server_info = piscsi_cmd.get_server_info()

    return response(
        template="admin.html",
        page_title=_("PiSCSI System Administration"),
        log_levels=server_info["log_levels"],
        current_log_level=server_info["current_log_level"],
        locales=get_supported_locales(),
        netatalk_configured=sys_cmd.running_proc("afpd"),
        samba_configured=sys_cmd.running_proc("smbd"),
        ftp_configured=sys_cmd.running_proc("vsftpd"),
        macproxy_configured=sys_cmd.running_proc("macproxy"),
        webmin_configured=sys_cmd.running_proc("miniserv.pl"),
    )


@APP.route("/upload", methods=["GET"])
def upload_page():
    """
    Sets up the data structures and kicks off the rendering of the file uploading page
    """
    server_info = piscsi_cmd.get_server_info()
    working_dirs_exist((server_info["image_dir"], CFG_DIR))

    return response(
        template="upload.html",
        page_title=_("PiSCSI File Upload"),
        is_root_page=True,
        images_subdirs=file_cmd.list_subdirs(server_info["image_dir"]),
        shared_subdirs=file_cmd.list_subdirs(FILE_SERVER_DIR),
        file_server_dir_exists=Path(FILE_SERVER_DIR).exists(),
        max_file_size=int(int(MAX_FILE_SIZE) / 1024 / 1024),
        CFG_DIR=CFG_DIR,
        FILE_SERVER_DIR=FILE_SERVER_DIR,
    )


@APP.route("/login", methods=["POST"])
def login():
    """
    Uses simplepam to authenticate against Linux users
    """
    username = request.form["username"]
    password = request.form["password"]
    groups = [g.gr_name for g in getgrall() if username in g.gr_mem]

    if AUTH_GROUP in groups and authenticate(str(username), str(password)):
        session["username"] = request.form["username"]
        return response(env=get_env_info())

    return response(
        error=True,
        status_code=401,
        message=_(
            "You must log in with valid credentials for a user in the '%(group)s' group",
            group=AUTH_GROUP,
        ),
    )


@APP.route("/logout")
def logout():
    """
    Removes the logged in user from the session
    """
    session.pop("username", None)
    return response()


@APP.route("/pwa/<path:pwa_path>")
def send_pwa_files(pwa_path):
    """
    Sets up mobile web resources
    """
    return send_from_directory("pwa", pwa_path)


def login_required(func):
    """
    Wrapper method for enabling authentication for an endpoint
    """

    @wraps(func)
    def decorated_function(*args, **kwargs):
        auth = auth_active(AUTH_GROUP)
        if auth["status"] and "username" not in session:
            return response(error=True, message=auth["msg"])
        return func(*args, **kwargs)

    return decorated_function


@APP.route("/drive/create", methods=["POST"])
@login_required
def drive_create():
    """
    Creates the image and properties file pair
    """
    drive_name = request.form.get("drive_name")
    file_name = Path(request.form.get("file_name"))

    properties = get_properties_by_drive_name(APP.config["PISCSI_DRIVE_PROPERTIES"], drive_name)

    if not properties:
        return response(
            error=True,
            message=_("No properties data for drive %(drive_name)s", drive_name=drive_name),
        )

    # Creating the image file
    server_info = piscsi_cmd.get_server_info()
    process = file_cmd.create_empty_image(
        Path(server_info["image_dir"]) / f"{file_name}.{properties['file_type']}",
        properties["size"],
    )
    process = ReturnCodeMapper.add_msg(process)
    if not process["status"]:
        return response(error=True, message=process["msg"])

    # Creating the drive properties file
    full_file_name = f"{file_name}.{properties['file_type']}"
    prop_file_name = f"{full_file_name}.{PROPERTIES_SUFFIX}"
    process = file_cmd.write_drive_properties(prop_file_name, properties)
    process = ReturnCodeMapper.add_msg(process)
    if not process["status"]:
        return response(error=True, message=process["msg"])

    return response(
        message=_(
            "Image file with properties created: %(file_name)s",
            file_name=full_file_name,
        )
    )


@APP.route("/drive/cdrom", methods=["POST"])
@login_required
def drive_cdrom():
    """
    Creates a properties file for a CD-ROM image
    """
    drive_name = request.form.get("drive_name")
    file_name = Path(request.form.get("file_name"))

    # Creating the drive properties file
    file_name = f"{file_name}.{PROPERTIES_SUFFIX}"
    properties = get_properties_by_drive_name(APP.config["PISCSI_DRIVE_PROPERTIES"], drive_name)

    if not properties:
        return response(
            error=True,
            message=_("No properties data for drive %(drive_name)s", drive_name=drive_name),
        )

    process = file_cmd.write_drive_properties(file_name, properties)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        return response(message=process["msg"])

    return response(error=True, message=process["msg"])


@APP.route("/config/save", methods=["POST"])
@login_required
def config_save():
    """
    Saves a config file to disk
    """
    file_name = Path(request.form.get("name") + f".{CONFIG_FILE_SUFFIX}").name

    process = file_cmd.write_config(file_name)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        return response(message=process["msg"])

    return response(error=True, message=process["msg"])


@APP.route("/config/action", methods=["POST"])
@login_required
def config_action():
    """
    Carries out on an operation on the config file
    """
    file_name = Path(request.form.get("name"))
    safe_path = is_safe_path(file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])

    if "load" in request.form:
        process = file_cmd.read_config(file_name)
        process = ReturnCodeMapper.add_msg(process)
        if process["status"]:
            return response(message=process["msg"])

        return response(error=True, message=process["msg"])

    if "delete" in request.form:
        process = file_cmd.delete_file(Path(CFG_DIR) / file_name)
        process = ReturnCodeMapper.add_msg(process)
        if process["status"]:
            return response(message=process["msg"])

        return response(error=True, message=process["msg"])

    if "send" in request.form:
        return send_from_directory(CFG_DIR, str(file_name), as_attachment=True)

    return response(
        error=True,
        message="No known operation in request header. Expected one of: load, delete, send",
    )


@APP.route("/files/diskinfo", methods=["POST"])
def show_diskinfo():
    """
    Displays disk image info
    """
    file_name = Path(request.form.get("file_name"))
    safe_path = is_safe_path(file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    server_info = piscsi_cmd.get_server_info()
    working_dirs_exist((server_info["image_dir"], CFG_DIR))
    returncode, diskinfo = sys_cmd.get_diskinfo(Path(server_info["image_dir"]) / file_name)
    if returncode == 0:
        return response(
            template="diskinfo.html",
            page_title=_("PiSCSI Image Info"),
            file_name=str(file_name),
            diskinfo=diskinfo,
        )

    return response(
        error=True,
        message=_("An error occurred when getting disk info: %(error)s", error=diskinfo),
    )


@APP.route("/sys/manpage", methods=["GET"])
def show_manpage():
    """
    Displays manpage
    """
    app_allowlist = ["piscsi", "scsictl", "scsidump", "scsimon"]

    app = request.args.get("app", type=str)

    if app not in app_allowlist:
        return response(error=True, message=_("%(app)s is not a recognized PiSCSI app", app=app))

    file_path = f"{WEB_DIR}/../../../doc/{app}.1"
    html_to_strip = [
        "Content-type",
        "!DOCTYPE",
        "<HTML>",
        "<HEAD>",
        "<BODY>",
        "<H1>",
    ]

    returncode, manpage = sys_cmd.get_manpage(file_path)
    if returncode == 0:
        formatted_manpage = ""
        for line in manpage.splitlines(True):
            # Make URIs compatible with the Flask webapp
            if "/?1+" in line:
                line = line.replace("/?1+", "manpage?app=")
            # Strip out useless hyperlink
            elif "man2html" in line:
                line = line.replace('<A HREF="/">man2html</A>', "man2html")
            if not any(ele in line for ele in html_to_strip):
                formatted_manpage += line

        return response(
            template="manpage.html",
            page_title=_("PiSCSI Manual"),
            app=app,
            manpage=formatted_manpage,
        )

    return response(
        error=True,
        message=_("An error occurred when accessing manual page: %(error)s", error=manpage),
    )


@APP.route("/logs/show", methods=["POST"])
def show_logs():
    """
    Displays system logs
    """
    lines = request.form.get("lines")
    scope = request.form.get("scope")

    returncode, logs = sys_cmd.get_logs(lines, scope)
    if returncode == 0:
        return response(
            template="logs.html",
            page_title=_("PiSCSI System Logs"),
            scope=scope,
            lines=lines,
            logs=logs,
        )

    return response(
        error=True,
        message=_("An error occurred when fetching logs: %(error)s", error=logs),
    )


@APP.route("/logs/level", methods=["POST"])
@login_required
def log_level():
    """
    Sets PiSCSI backend log level
    """
    level = request.form.get("level") or "info"

    process = piscsi_cmd.set_log_level(level)
    if process["status"]:
        return response(message=_("Log level set to %(value)s", value=level))

    return response(error=True, message=process["msg"])


@APP.route("/scsi/attach_device", methods=["POST"])
@login_required
def attach_device():
    """
    Attaches a peripheral device that doesn't take an image file as argument
    """
    scsi_id = request.form.get("scsi_id")
    unit = request.form.get("unit")
    device_type = request.form.get("type")
    drive_name = request.form.get("drive_name")

    if not scsi_id:
        return response(error=True, message=_("No SCSI ID specified"))

    # Attempt to fetch the drive properties based on drive name
    drive_props = None
    if drive_name:
        for drive in APP.config["PISCSI_DRIVE_PROPERTIES"]:
            if drive["name"] == drive_name:
                drive_props = drive
                break

    # Collect device parameters into a dictionary
    PARAM_PREFIX = "param_"
    params = {}
    for item in request.form:
        if item.startswith(PARAM_PREFIX):
            param = request.form.get(item)
            if param:
                params.update({item.replace(PARAM_PREFIX, ""): param})

    return_message = "Attached %(device_type)s to SCSI ID %(id_number)s LUN %(unit_number)s"
    if "interface" in params.keys():
        bridge_status = is_bridge_configured(params["interface"])
        if not bridge_status["status"]:
            return response(error=True, message=bridge_status["msg"])
        return_message = return_message + " - " + bridge_status["msg"]

    kwargs = {
        "unit": int(unit),
        "device_type": device_type,
        "params": params,
    }
    if drive_props:
        kwargs["vendor"] = drive_props["vendor"]
        kwargs["product"] = drive_props["product"]
        kwargs["revision"] = drive_props["revision"]
        kwargs["block_size"] = drive_props["block_size"]

    process = piscsi_cmd.attach_device(scsi_id, **kwargs)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        return response(
            message=_(
                return_message,
                device_type=get_device_name(device_type),
                id_number=scsi_id,
                unit_number=unit,
            )
        )

    return response(error=True, message=process["msg"])


@APP.route("/scsi/attach", methods=["POST"])
@login_required
def attach_image():
    """
    Attaches a file image as a device
    """
    file_name = request.form.get("file_name")
    file_size = request.form.get("file_size")
    scsi_id = request.form.get("scsi_id")
    unit = request.form.get("unit")
    device_type = request.form.get("type")

    if not scsi_id:
        return response(error=True, message=_("No SCSI ID specified"))
    if not file_name:
        return response(error=True, message=_("No image file to insert"))

    kwargs = {"unit": int(unit), "params": {"file": file_name}}

    if device_type:
        kwargs["device_type"] = device_type
        device_types = piscsi_cmd.get_device_types()
        expected_block_size = min(device_types["device_types"][device_type]["block_sizes"])

    # Attempt to load the device properties file:
    # same file name with PROPERTIES_SUFFIX appended
    drive_properties = Path(CFG_DIR) / f"{file_name}.{PROPERTIES_SUFFIX}"
    if drive_properties.is_file():
        process = file_cmd.read_drive_properties(drive_properties)
        process = ReturnCodeMapper.add_msg(process)
        if not process["status"]:
            return response(error=True, message=process["msg"])
        conf = process["conf"]
        kwargs["vendor"] = conf["vendor"]
        kwargs["product"] = conf["product"]
        kwargs["revision"] = conf["revision"]
        kwargs["block_size"] = conf["block_size"]
        expected_block_size = conf["block_size"]

    process = piscsi_cmd.attach_device(scsi_id, **kwargs)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        if int(file_size) % int(expected_block_size):
            logging.warning(
                "The image file size %s bytes is not a multiple of %s. "
                "PiSCSI will ignore the trailing data. "
                "The image may be corrupted, so proceed with caution.",
                file_size,
                expected_block_size,
            )
        return response(
            message=_(
                "Attached %(file_name)s as %(device_type)s to "
                "SCSI ID %(id_number)s LUN %(unit_number)s",
                file_name=file_name,
                device_type=get_device_name(device_type),
                id_number=scsi_id,
                unit_number=unit,
            )
        )

    return response(error=True, message=process["msg"])


@APP.route("/scsi/detach_all", methods=["POST"])
@login_required
def detach_all_devices():
    """
    Detaches all currently attached devices
    """
    process = piscsi_cmd.detach_all()
    if process["status"]:
        return response(message=_("Detached all SCSI devices"))

    return response(error=True, message=process["msg"])


@APP.route("/scsi/detach", methods=["POST"])
@login_required
def detach():
    """
    Detaches a specified device
    """
    scsi_id = request.form.get("scsi_id")
    unit = request.form.get("unit")
    process = piscsi_cmd.detach_by_id(scsi_id, unit)
    if process["status"]:
        return response(
            message=_(
                "Detached SCSI ID %(id_number)s LUN %(unit_number)s",
                id_number=scsi_id,
                unit_number=unit,
            )
        )

    return response(error=True, message=process["msg"])


@APP.route("/scsi/eject", methods=["POST"])
@login_required
def eject():
    """
    Ejects a specified removable device image, but keeps the device attached
    """
    scsi_id = request.form.get("scsi_id")
    unit = request.form.get("unit")

    process = piscsi_cmd.eject_by_id(scsi_id, unit)
    if process["status"]:
        return response(
            message=_(
                "Ejected SCSI ID %(id_number)s LUN %(unit_number)s",
                id_number=scsi_id,
                unit_number=unit,
            )
        )

    return response(error=True, message=process["msg"])


@APP.route("/scsi/info", methods=["POST"])
def device_info():
    """
    Displays detailed info for all attached devices
    """
    process = piscsi_cmd.list_devices()
    if process["status"]:
        return response(
            template="deviceinfo.html",
            page_title=_("PiSCSI Device Info"),
            devices=process["device_list"],
        )

    return response(error=True, message=_("No devices attached"))


@APP.route("/scsi/reserve", methods=["POST"])
@login_required
def reserve_id():
    """
    Reserves a SCSI ID and stores the memo for that reservation
    """
    scsi_id = request.form.get("scsi_id")
    memo = request.form.get("memo")
    reserved_ids = piscsi_cmd.get_reserved_ids()["ids"]
    reserved_ids.extend(scsi_id)
    process = piscsi_cmd.reserve_scsi_ids(reserved_ids)
    if process["status"]:
        RESERVATIONS[int(scsi_id)] = memo
        return response(message=_("Reserved SCSI ID %(id_number)s", id_number=scsi_id))

    return response(error=True, message=process["msg"])


@APP.route("/scsi/release", methods=["POST"])
@login_required
def release_id():
    """
    Releases the reservation of a SCSI ID as well as the memo for the reservation
    """
    scsi_id = request.form.get("scsi_id")
    reserved_ids = piscsi_cmd.get_reserved_ids()["ids"]
    reserved_ids.remove(scsi_id)
    process = piscsi_cmd.reserve_scsi_ids(reserved_ids)
    if process["status"]:
        RESERVATIONS[int(scsi_id)] = ""
        return response(
            message=_("Released the reservation for SCSI ID %(id_number)s", id_number=scsi_id)
        )

    return response(error=True, message=process["msg"])


@APP.route("/sys/rename", methods=["POST"])
@login_required
def rename_system():
    """
    Changes the hostname of the system
    """
    name = str(request.form.get("system_name"))
    max_length = 120

    if len(name) <= max_length:
        process = sys_cmd.set_pretty_host(name)
        if process:
            if name:
                return response(message=_("System name changed to '%(name)s'.", name=name))
            return response(message=_("System name reset to default."))

    return response(error=True, message=_("Failed to change system name."))


@APP.route("/sys/reboot", methods=["POST"])
@login_required
def restart():
    """
    Restarts the system
    """
    returncode, message = sys_cmd.reboot_system()
    if not returncode:
        return response()

    return response(error=True, message=message)


@APP.route("/sys/shutdown", methods=["POST"])
@login_required
def shutdown():
    """
    Shuts down the system
    """
    returncode, message = sys_cmd.shutdown_system()
    if not returncode:
        return response()

    return response(error=True, message=message)


@APP.route("/files/create_iso", methods=["POST"])
@login_required
def download_to_iso():
    """
    Downloads a file and creates a CD-ROM image with the specified file system and the file
    """
    url = request.form.get("url")
    iso_type = request.form.get("type")
    local_file = request.form.get("file")

    if iso_type == "HFS":
        # The file genisoimage_hfs_resource_fork_map.txt is part of the piscsi
        # repository tree, so it should be present in the parent folder; trust but verify:
        genisoimage_hfs_resource_fork_map_file_path = Path(
            f"{WEB_DIR}/../genisoimage_hfs_resource_fork_map.txt"
        )
        if genisoimage_hfs_resource_fork_map_file_path.exists():
            # genisoimage will look up the file extension in this map file to
            # derive the file's CREATOR and TYPE resource fork attributes.
            # See more at https://linux.die.net/man/1/genisoimage
            iso_args = ["-hfs", "-map", str(genisoimage_hfs_resource_fork_map_file_path)]
            logging.info(
                "Found and using the genisoimage hfs map file at %s",
                str(genisoimage_hfs_resource_fork_map_file_path),
            )
        else:
            logging.warning(
                "The genisoimage hfs map file is not present at %s. "
                "Will not set resource fork attributes of files in the iso image!",
                str(genisoimage_hfs_resource_fork_map_file_path),
            )
            iso_args = ["-hfs"]
    elif iso_type == "ISO-9660 Level 1":
        iso_args = ["-iso-level", "1"]
    elif iso_type == "ISO-9660 Level 2":
        iso_args = ["-iso-level", "2"]
    elif iso_type == "ISO-9660 Level 3":
        iso_args = ["-iso-level", "3"]
    elif iso_type == "Joliet":
        iso_args = ["-J"]
    elif iso_type == "Rock Ridge":
        iso_args = ["-r"]
    else:
        return response(
            error=True,
            message=_("%(iso_type)s is not a valid CD-ROM format.", iso_type=iso_type),
        )

    if url:
        process = file_cmd.download_file_to_iso(url, *iso_args)
    elif local_file:
        server_info = piscsi_cmd.get_server_info()
        file_path = Path(server_info["image_dir"]) / local_file
        iso_path = Path(str(file_path) + ".iso")
        process = file_cmd.generate_iso(iso_path, file_path, *iso_args)

    process = ReturnCodeMapper.add_msg(process)
    if not process["status"]:
        return response(
            error=True,
            message=_(
                "The following error occurred when creating the CD-ROM image: %(error)s",
                error=process["msg"],
            ),
        )

    return response(
        message=_(
            "CD-ROM image %(file_name)s with type %(iso_type)s was created.",
            file_name=process["file_name"],
            iso_type=iso_type,
        ),
    )


@APP.route("/files/download_url", methods=["POST"])
@login_required
def download_file():
    """
    Downloads a remote file onto the images dir on the system
    """
    destination = request.form.get("destination")
    url = request.form.get("url")
    images_subdir = request.form.get("images_subdir")
    shared_subdir = request.form.get("shared_subdir")
    if destination == "disk_images":
        safe_path = is_safe_path(Path(images_subdir))
        if not safe_path["status"]:
            return response(error=True, message=safe_path["msg"])
        server_info = piscsi_cmd.get_server_info()
        destination_dir = Path(server_info["image_dir"]) / images_subdir
    elif destination == "shared_files":
        safe_path = is_safe_path(Path(shared_subdir))
        if not safe_path["status"]:
            return response(error=True, message=safe_path["msg"])
        destination_dir = Path(FILE_SERVER_DIR) / shared_subdir
    else:
        return response(error=True, message=_("Unknown destination"))

    process = file_cmd.download_to_dir(url, Path(destination_dir) / Path(url).name)
    process = ReturnCodeMapper.add_msg(process)
    if process["status"]:
        return response(message=process["msg"])

    return response(
        error=True,
        message=_(
            "The following error occurred when downloading: %(error)s",
            error=process["msg"],
        ),
    )


@APP.route("/files/upload", methods=["POST"])
def upload_file():
    """
    Uploads a file from the local computer to the images dir on the system
    Depending on the Dropzone.js JavaScript library
    """
    # Due to the embedded javascript library, we cannot use the @login_required decorator
    auth = auth_active(AUTH_GROUP)
    if auth["status"] and "username" not in session:
        return make_response(auth["msg"], 403)

    destination = request.form.get("destination")
    images_subdir = request.form.get("images_subdir")
    shared_subdir = request.form.get("shared_subdir")
    if destination == "disk_images":
        safe_path = is_safe_path(Path(images_subdir))
        if not safe_path["status"]:
            return make_response(safe_path["msg"], 403)
        server_info = piscsi_cmd.get_server_info()
        destination_dir = Path(server_info["image_dir"]) / images_subdir
    elif destination == "shared_files":
        safe_path = is_safe_path(Path(shared_subdir))
        if not safe_path["status"]:
            return make_response(safe_path["msg"], 403)
        destination_dir = Path(FILE_SERVER_DIR) / shared_subdir
    elif destination == "piscsi_config":
        destination_dir = Path(CFG_DIR)
    else:
        return make_response(_("Unknown destination"), 403)

    log = logging.getLogger("pydrop")
    file_object = request.files["file"]
    file_name = secure_filename(file_object.filename)
    tmp_file_name = "__tmp_" + file_name

    save_path = destination_dir / file_name
    tmp_save_path = destination_dir / tmp_file_name
    current_chunk = int(request.form["dzchunkindex"])

    # Makes sure not to overwrite an existing file,
    # but continues writing to a file transfer in progress
    if path.exists(save_path) and current_chunk == 0:
        return make_response(_("The file already exists!"), 400)

    if path.exists(tmp_save_path) and current_chunk == 0:
        log.info("Deleting existing temporary file before uploading...")
        file_cmd.delete_file(tmp_save_path)

    try:
        with open(tmp_save_path, "ab") as save:
            save.seek(int(request.form["dzchunkbyteoffset"]))
            save.write(file_object.stream.read())
    except OSError:
        log.exception("Could not write to file")
        return make_response(_("Unable to write the file to disk!"), 500)

    total_chunks = int(request.form["dztotalchunkcount"])

    if current_chunk + 1 == total_chunks:
        # Validate the resulting file size after writing the last chunk
        if path.getsize(tmp_save_path) != int(request.form["dztotalfilesize"]):
            log.error("File size mismatch between the original file and transferred file.")
            return make_response(_("Transferred file corrupted!"), 500)

        process = file_cmd.rename_file(Path(tmp_save_path), Path(save_path))
        if not process["status"]:
            return make_response(_("Unable to rename temporary file!"), 500)

    return make_response(_("File upload successful!"), 200)


@APP.route("/files/create", methods=["POST"])
@login_required
def create_file():
    """
    Creates an empty image file in the images dir
    """
    file_name = Path(request.form.get("file_name"))
    size = int(request.form.get("size")) * 1024 * 1024
    file_type = request.form.get("type")
    drive_name = request.form.get("drive_name")
    drive_format = request.form.get("drive_format")

    safe_path = is_safe_path(file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    full_file_name = f"{file_name}.{file_type}"

    server_info = piscsi_cmd.get_server_info()
    process = file_cmd.create_empty_image(Path(server_info["image_dir"]) / full_file_name, size)
    process = ReturnCodeMapper.add_msg(process)
    if not process["status"]:
        return response(error=True, message=process["msg"])

    message_postfix = ""

    # Formatting and injecting driver, if one is chosen
    if drive_format:
        volume_name = f"HD {size / 1024 / 1024:0.0f}M"
        known_formats = [
            "Lido 7.56",
            "SpeedTools 3.6",
            "FAT16",
            "FAT32",
        ]
        message_postfix = f" ({drive_format})"

        if drive_format not in known_formats:
            return response(
                error=True,
                message=_(
                    "%(drive_format)s is not a valid hard disk format.",
                    drive_format=drive_format,
                ),
            )
        elif drive_format.startswith("FAT"):
            if drive_format == "FAT16":
                fat_size = "16"
            elif drive_format == "FAT32":
                fat_size = "32"
            else:
                return response(
                    error=True,
                    message=_(
                        "%(drive_format)s is not a valid hard disk format.",
                        drive_format=drive_format,
                    ),
                )

            process = file_cmd.partition_disk(full_file_name, volume_name, "FAT")
            if not process["status"]:
                return response(error=True, message=process["msg"])

            process = file_cmd.format_fat(
                full_file_name,
                # FAT volume labels are max 11 chars
                volume_name[:11],
                fat_size,
            )
            if not process["status"]:
                return response(error=True, message=process["msg"])

        else:
            driver_base_path = Path(f"{WEB_DIR}/../../../mac-hard-disk-drivers")
            process = file_cmd.partition_disk(full_file_name, volume_name, "HFS")
            if not process["status"]:
                return response(error=True, message=process["msg"])

            process = file_cmd.format_hfs(
                full_file_name,
                volume_name,
                driver_base_path / Path(drive_format.replace(" ", "-") + ".img"),
            )
            if not process["status"]:
                return response(error=True, message=process["msg"])

    # Creating the drive properties file, if one is chosen
    if drive_name:
        properties = get_properties_by_drive_name(APP.config["PISCSI_DRIVE_PROPERTIES"], drive_name)
        if properties:
            prop_file_name = f"{full_file_name}.{PROPERTIES_SUFFIX}"
            process = file_cmd.write_drive_properties(prop_file_name, properties)
            process = ReturnCodeMapper.add_msg(process)
            if not process["status"]:
                return response(error=True, message=process["msg"])

            return response(
                status_code=201,
                message=_(
                    "Image file with properties created: %(file_name)s%(drive_format)s",
                    file_name=full_file_name,
                    drive_format=message_postfix,
                ),
                image=full_file_name,
            )

    return response(
        status_code=201,
        message=_(
            "Image file created: %(file_name)s%(drive_format)s",
            file_name=full_file_name,
            drive_format=message_postfix,
        ),
        image=full_file_name,
    )


@APP.route("/files/download_image", methods=["POST"])
@login_required
def download_image():
    """
    Downloads a file from the image dir to the local computer
    """
    file_name = Path(request.form.get("file"))
    safe_path = is_safe_path(file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    server_info = piscsi_cmd.get_server_info()
    return send_from_directory(server_info["image_dir"], str(file_name), as_attachment=True)


@APP.route("/files/download_config", methods=["POST"])
@login_required
def download_config():
    """
    Downloads a file from the config dir to the local computer
    """
    file_name = Path(request.form.get("file"))
    safe_path = is_safe_path(file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    return send_from_directory(CFG_DIR, str(file_name), as_attachment=True)


@APP.route("/files/delete", methods=["POST"])
@login_required
def delete():
    """
    Deletes a specified file in the images dir
    """
    file_name = Path(request.form.get("file_name"))
    safe_path = is_safe_path(file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    server_info = piscsi_cmd.get_server_info()
    process = file_cmd.delete_file(Path(server_info["image_dir"]) / file_name)
    process = ReturnCodeMapper.add_msg(process)
    if not process["status"]:
        return response(error=True, message=process["msg"])

    # Delete the drive properties file, if it exists
    prop_file_path = Path(CFG_DIR) / f"{file_name}.{PROPERTIES_SUFFIX}"
    if prop_file_path.is_file():
        process = file_cmd.delete_file(prop_file_path)
        process = ReturnCodeMapper.add_msg(process)
        if process["status"]:
            return response(
                message=_(
                    "Image file with properties deleted: %(file_name)s",
                    file_name=str(file_name),
                ),
            )
        else:
            return response(error=True, message=process["msg"])

    return response(
        message=_(
            "Image file deleted: %(file_name)s",
            file_name=str(file_name),
        ),
    )


@APP.route("/files/rename", methods=["POST"])
@login_required
def rename():
    """
    Renames a specified file in the images dir
    """
    file_name = Path(request.form.get("file_name"))
    new_file_name = Path(request.form.get("new_file_name"))
    safe_path = is_safe_path(file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    safe_path = is_safe_path(new_file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    server_info = piscsi_cmd.get_server_info()
    process = file_cmd.rename_file(
        Path(server_info["image_dir"]) / str(file_name),
        Path(server_info["image_dir"]) / str(new_file_name),
    )
    process = ReturnCodeMapper.add_msg(process)
    if not process["status"]:
        return response(error=True, message=process["msg"])

    # Rename the drive properties file, if it exists
    prop_file_path = Path(CFG_DIR) / f"{file_name}.{PROPERTIES_SUFFIX}"
    new_prop_file_path = Path(CFG_DIR) / f"{new_file_name}.{PROPERTIES_SUFFIX}"
    if prop_file_path.is_file():
        process = file_cmd.rename_file(prop_file_path, new_prop_file_path)
        process = ReturnCodeMapper.add_msg(process)
        if process["status"]:
            return response(
                message=_(
                    "Image file with properties renamed to: %(file_name)s",
                    file_name=str(new_file_name),
                ),
            )
        else:
            return response(error=True, message=process["msg"])

    return response(
        message=_(
            "Image file renamed to: %(file_name)s",
            file_name=str(new_file_name),
        ),
    )


@APP.route("/files/copy", methods=["POST"])
@login_required
def copy():
    """
    Creates a copy of a specified file in the images dir
    """
    file_name = Path(request.form.get("file_name"))
    new_file_name = Path(request.form.get("copy_file_name"))
    safe_path = is_safe_path(file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    safe_path = is_safe_path(new_file_name)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    server_info = piscsi_cmd.get_server_info()
    process = file_cmd.copy_file(
        Path(server_info["image_dir"]) / str(file_name),
        Path(server_info["image_dir"]) / str(new_file_name),
    )
    process = ReturnCodeMapper.add_msg(process)
    if not process["status"]:
        return response(error=True, message=process["msg"])

    # Create a copy of the drive properties file, if it exists
    prop_file_path = Path(CFG_DIR) / f"{file_name}.{PROPERTIES_SUFFIX}"
    new_prop_file_path = Path(CFG_DIR) / f"{new_file_name}.{PROPERTIES_SUFFIX}"
    if prop_file_path.is_file():
        process = file_cmd.copy_file(prop_file_path, new_prop_file_path)
        process = ReturnCodeMapper.add_msg(process)
        if process["status"]:
            return response(
                message=_(
                    "Copy of image file with properties saved as: %(file_name)s",
                    file_name=str(new_file_name),
                ),
            )
        else:
            return response(error=True, message=process["msg"])

    return response(
        message=_(
            "Copy of image file saved as: %(file_name)s",
            file_name=str(new_file_name),
        ),
    )


@APP.route("/files/extract_image", methods=["POST"])
@login_required
def extract_image():
    """
    Extracts all or a subset of files in the specified archive
    """
    archive_file = Path(request.form.get("archive_file"))
    archive_members_raw = request.form.get("archive_members") or None
    archive_members = archive_members_raw.split("|") if archive_members_raw else None
    safe_path = is_safe_path(archive_file)
    if not safe_path["status"]:
        return response(error=True, message=safe_path["msg"])
    extract_result = file_cmd.extract_image(str(archive_file), archive_members)

    if extract_result["return_code"] == ReturnCodes.EXTRACTIMAGE_SUCCESS:

        for properties_file in extract_result["properties_files_moved"]:
            if properties_file["status"]:
                logging.info(
                    "Properties file %s moved to %s",
                    properties_file["name"],
                    CFG_DIR,
                )
            else:
                logging.warning(
                    "Failed to move properties file %s to %s",
                    properties_file["name"],
                    CFG_DIR,
                )

        return response(message=ReturnCodeMapper.add_msg(extract_result).get("msg"))

    return response(error=True, message=ReturnCodeMapper.add_msg(extract_result).get("msg"))


@APP.route("/language", methods=["POST"])
def change_language():
    """
    Changes the session language locale and refreshes the Flask app context
    """
    locale = request.form.get("locale")
    session["language"] = locale
    piscsi_cmd.locale = session["language"]
    file_cmd.locale = session["language"]
    refresh()

    language = Locale.parse(locale)
    language_name = language.get_language_name(locale)
    return response(message=_("Changed Web Interface language to %(locale)s", locale=language_name))


@APP.route("/theme", methods=["GET", "POST"])
def change_theme():
    if request.method == "GET":
        theme = request.args.get("name")
    else:
        theme = request.form.get("name")

    if theme not in TEMPLATE_THEMES:
        return response(error=True, message=_("The requested theme does not exist."))

    session["theme"] = theme
    return response(message=_("Theme changed to '%(theme)s'.", theme=theme))


@APP.route("/healthcheck", methods=["GET"])
def healthcheck():
    return "", 200


@APP.before_request
def detect_locale():
    """
    Get the detected locale to use for UI string translations.
    Assign the language string to objects to be used for requests.
    """
    if "language" not in session.keys():
        session["language"] = get_locale()

    piscsi_cmd.locale = session["language"]
    file_cmd.locale = session["language"]


@APP.before_request
def log_http_request():
    if logging.getLogger().isEnabledFor(logging.DEBUG):
        message = f"HTTP request: {request.method} {request.path}"

        if request.method == "POST":
            if request.path == "/login":
                message += " (<hidden>)"
            elif len(request.get_data()) > 100:
                message += f" (payload: {request.get_data()[:100]} <truncated>)"
            else:
                message += f" (payload: {request.get_data()})"

        logging.debug(message)


@APP.before_request
def check_backend_auth():
    if not piscsi_cmd.is_token_auth()["status"] and not APP.config["PISCSI_TOKEN"]:
        abort(
            403,
            "PiSCSI is password protected. "
            "Start the Web Interface with the --password parameter.",
        )


if __name__ == "__main__":
    APP.secret_key = "piscsi_is_awesome_insecure_secret_key"
    APP.config["SESSION_TYPE"] = "filesystem"
    APP.config["MAX_CONTENT_LENGTH"] = int(MAX_FILE_SIZE)

    parser = argparse.ArgumentParser(description="PiSCSI Web Interface command line arguments")
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
        help="Token password string for authenticating with PiSCSI",
    )
    parser.add_argument(
        "--backend-host",
        type=str,
        default="localhost",
        action="store",
        help="PiSCSI backend hostname. Default: localhost",
    )
    parser.add_argument(
        "--backend-port",
        type=int,
        default=6868,
        action="store",
        help="PiSCSI backend port number. Default: 6868",
    )
    parser.add_argument(
        "--log-level",
        type=str,
        default="warning",
        action="store",
        help="Log level for Web UI. Default: warning",
        choices=["debug", "info", "warning", "error", "critical"],
    )
    parser.add_argument(
        "--dev-mode",
        action="store_true",
        help="Run in development mode",
    )

    arguments = parser.parse_args()
    APP.config["PISCSI_TOKEN"] = arguments.password

    logging.config.dictConfig(
        {
            "version": 1,
            "formatters": {
                "default": {
                    "format": "[%(asctime)s] [%(levelname)s] %(filename)s:%(lineno)s %(message)s",
                }
            },
            "handlers": {
                "wsgi": {
                    "class": "logging.StreamHandler",
                    "formatter": "default",
                }
            },
            "root": {
                "level": arguments.log_level.upper(),
                "handlers": ["wsgi"],
            },
        }
    )

    sock_cmd = SocketCmdsFlask(host=arguments.backend_host, port=arguments.backend_port)
    piscsi_cmd = PiscsiCmds(sock_cmd=sock_cmd, token=APP.config["PISCSI_TOKEN"])
    file_cmd = FileCmds(piscsi=piscsi_cmd)
    sys_cmd = SysCmds()

    if Path(f"{CFG_DIR}/{DEFAULT_CONFIG}").is_file():
        file_cmd.read_config(DEFAULT_CONFIG)
    if Path(f"{DRIVE_PROPERTIES_FILE}").is_file():
        process = file_cmd.read_drive_properties(DRIVE_PROPERTIES_FILE)
        if process["status"]:
            APP.config["PISCSI_DRIVE_PROPERTIES"] = process["conf"]
        else:
            APP.config["PISCSI_DRIVE_PROPERTIES"] = []
            logging.error(process["msg"])
    else:
        APP.config["PISCSI_DRIVE_PROPERTIES"] = []
        logging.warning("Could not read drive properties from %s", DRIVE_PROPERTIES_FILE)

    logging.info("Starting WSGI server...")
    if arguments.dev_mode:
        logging.info("Dev mode enabled")
        APP.debug = True
        from werkzeug.debug import DebuggedApplication

        try:
            bjoern.run(DebuggedApplication(APP, evalex=False), "0.0.0.0", arguments.port)
        except KeyboardInterrupt:
            pass
    else:
        bjoern.run(APP, "0.0.0.0", arguments.port)
