"""
Module for the Flask app rendering and endpoints
"""

import logging
from pathlib import Path
from google.protobuf.pyext._message import RepeatedScalarContainer, ScalarMapContainer
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
    jsonify, get_flashed_messages
)

from file_cmds import (
    list_images,
    list_config_files,
    create_new_image,
    download_file_to_iso,
    delete_image,
    delete_file,
    unzip_file,
    download_to_dir,
    write_config,
    read_config,
    write_drive_properties,
    read_drive_properties,
)
from pi_cmds import (
    shutdown_pi,
    reboot_pi,
    running_env,
    systemd_service,
    running_netatalk,
    is_bridge_setup,
    disk_space,
)
from ractl_cmds import (
    attach_image,
    list_devices,
    detach_by_id,
    eject_by_id,
    detach_all,
    get_server_info,
    get_reserved_ids,
    get_network_info,
    get_device_types,
    reserve_scsi_ids,
    set_log_level,
)
from device_utils import (
    sort_and_format_devices,
    get_valid_scsi_ids,
)
from settings import (
    CFG_DIR,
    AFP_DIR,
    MAX_FILE_SIZE,
    ARCHIVE_FILE_SUFFIX,
    CONFIG_FILE_SUFFIX,
    PROPERTIES_SUFFIX,
    DEFAULT_CONFIG,
    DRIVE_PROPERTIES_FILE,
    REMOVABLE_DEVICE_TYPES,
    RESERVATIONS,
)

APP = Flask(__name__)


@APP.route("/")
def index():
    """
    Sets up data structures for and renders the index page
    """
    server_info = get_server_info()
    disk = disk_space()
    devices = list_devices()
    device_types = get_device_types()
    files = list_images()
    config_files = list_config_files()

    sorted_image_files = sorted(files["files"], key=lambda x: x["name"].lower())
    sorted_config_files = sorted(config_files, key=lambda x: x.lower())

    attached_images = []
    units = 0
    # If there are more than 0 logical unit numbers, display in the Web UI
    for device in devices["device_list"]:
        attached_images.append(Path(device["image"]).name)
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

    return content_aware_response(
        "index.html",
        bridge_configured=is_bridge_setup(),
        netatalk_configured=running_netatalk(),
        devices=formatted_devices,
        files=sorted_image_files,
        config_files=sorted_config_files,
        base_dir=server_info["image_dir"],
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
        netinfo=get_network_info(),
        device_types=device_types["device_types"],
        free_disk=int(disk["free"] / 1024 / 1024),
        valid_file_suffix=valid_file_suffix,
        archive_file_suffix=ARCHIVE_FILE_SUFFIX,
        REMOVABLE_DEVICE_TYPES=REMOVABLE_DEVICE_TYPES,
    )


@APP.route("/drive/list", methods=["GET"])
def drive_list():
    """
    Sets up the data structures and kicks off the rendering of the drive list page
    """
    server_info = get_server_info()
    disk = disk_space()

    # Reads the canonical drive properties into a dict
    # The file resides in the current dir of the web ui process
    drive_properties = Path(DRIVE_PROPERTIES_FILE)
    if drive_properties.is_file():
        process = read_drive_properties(str(drive_properties))
        if not process["status"]:
            flash(process["msg"], "error")
            return content_aware_redirect("index")
        conf = process["conf"]
    else:
        flash("Could not read drive properties from " + str(drive_properties), "error")
        return content_aware_redirect("index")

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

    files = list_images()
    sorted_image_files = sorted(files["files"], key=lambda x: x["name"].lower())
    hd_conf = sorted(hd_conf, key=lambda x: x["name"].lower())
    cd_conf = sorted(cd_conf, key=lambda x: x["name"].lower())
    rm_conf = sorted(rm_conf, key=lambda x: x["name"].lower())

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
    )


@APP.route('/pwa/<path:path>')
def send_pwa_files(path):
    """
    Sets up mobile web resources
    """
    return send_from_directory('pwa', path)


@APP.route("/drive/create", methods=["POST"])
def drive_create():
    """
    Creates the image and properties file pair
    """
    vendor = get_param("vendor")
    product = get_param("product")
    revision = get_param("revision")
    block_size = get_param("block_size")
    size = get_param("size")
    file_type = get_param("file_type")
    file_name = get_param("file_name")

    # Creating the image file
    process = create_new_image(file_name, file_type, size)
    if process["status"]:
        flash(f"Created drive image file: {file_name}.{file_type}")
    else:
        flash(process["msg"], "error")
        return content_aware_redirect("index")

    # Creating the drive properties file
    prop_file_name = f"{file_name}.{file_type}.{PROPERTIES_SUFFIX}"
    properties = {
                "vendor": vendor,
                "product": product,
                "revision": revision,
                "block_size": block_size,
                }
    process = write_drive_properties(prop_file_name, properties)
    if process["status"]:
        flash(process["msg"])
        return content_aware_redirect("index")

    flash(process['msg'], "error")
    return content_aware_redirect("index")


@APP.route("/drive/cdrom", methods=["POST"])
def drive_cdrom():
    """
    Creates a properties file for a CD-ROM image
    """
    vendor = get_param("vendor")
    product = get_param("product")
    revision = get_param("revision")
    block_size = get_param("block_size")
    file_name = get_param("file_name")

    # Creating the drive properties file
    file_name = f"{file_name}.{PROPERTIES_SUFFIX}"
    properties = {
        "vendor": vendor,
        "product": product,
        "revision": revision,
        "block_size": block_size,
        }
    process = write_drive_properties(file_name, properties)
    if process["status"]:
        flash(process["msg"])
        return content_aware_redirect("index")

    flash(process['msg'], "error")
    return content_aware_redirect("index")


@APP.route("/config/save", methods=["POST"])
def config_save():
    """
    Saves a config file to disk
    """
    file_name = get_param("name", "default")
    file_name = f"{file_name}.{CONFIG_FILE_SUFFIX}"

    process = write_config(file_name)
    if process["status"]:
        flash(process["msg"])
        return content_aware_redirect("index")

    flash(process['msg'], "error")
    return content_aware_redirect("index")


@APP.route("/config/load", methods=["POST"])
def config_load():
    """
    Loads a config file from disk
    """
    file_name = get_param("name")

    if "load" in request.form:
        process = read_config(file_name)
        if process["status"]:
            flash(process["msg"])
            return content_aware_redirect("index")

        flash(process['msg'], "error")
        return content_aware_redirect("index")
    elif "delete" in request.form:
        process = delete_file(CFG_DIR + file_name)
        if process["status"]:
            flash(process["msg"])
            return content_aware_redirect("index")

        flash(process['msg'], "error")
        return content_aware_redirect("index")

    flash("Got an unhandled request (needs to be either load or delete)", "error")
    return content_aware_redirect("index")


@APP.route("/logs/show", methods=["POST"])
def show_logs():
    """
    Displays system logs
    """
    lines = get_param("lines", "200")
    scope = get_param("scope", "default")

    from subprocess import run
    if scope != "default":
        process = run(["journalctl", "-n", lines, "-u", scope], capture_output=True)
    else:
        process = run(["journalctl", "-n", lines], capture_output=True)

    if process.returncode == 0:
        headers = {"content-type": "text/plain"}
        return process.stdout.decode("utf-8"), int(lines), headers

    flash("Failed to get logs")
    flash(process.stdout.decode("utf-8"), "stdout")
    flash(process.stderr.decode("utf-8"), "stderr")
    return content_aware_redirect("index")


@APP.route("/logs/level", methods=["POST"])
def log_level():
    """
    Sets RaSCSI backend log level
    """
    level = get_param("level", "info")

    process = set_log_level(level)
    if process["status"]:
        flash(f"Log level set to {level}")
        return content_aware_redirect("index")

    flash(f"Failed to set log level to {level}!", "error")
    return content_aware_redirect("index")


@APP.route("/daynaport/attach", methods=["POST"])
def daynaport_attach():
    """
    Attaches a DaynaPORT ethernet adapter device
    """
    scsi_id = get_param("scsi_id")
    interface = get_param("if")
    ip_addr = get_param("ip")
    mask = get_param("mask")

    kwargs = {"device_type": "SCDP"}
    if interface != "":
        arg = interface
        if "" not in (ip_addr, mask):
            arg += (":" + ip_addr + "/" + mask)
        kwargs["interfaces"] = arg

    process = attach_image(scsi_id, **kwargs)
    if process["status"]:
        flash(f"Attached DaynaPORT to SCSI ID {scsi_id}!")
        return content_aware_redirect("index")

    flash(process["msg"], "error")
    return content_aware_redirect("index")


@APP.route("/scsi/attach", methods=["POST"])
def attach():
    """
    Attaches a file image as a device
    """
    file_name = get_param("name")
    file_size = get_param("size")
    scsi_id = get_param("scsi_id")
    unit = get_param("unit", 0)
    device_type = get_param("detected_type")

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
    drive_properties = f"{CFG_DIR}{file_name}.{PROPERTIES_SUFFIX}"
    if Path(drive_properties).is_file():
        process = read_drive_properties(drive_properties)
        if not process["status"]:
            flash(process["msg"], "error")
            return content_aware_redirect("index")
        conf = process["conf"]
        kwargs["vendor"] = conf["vendor"]
        kwargs["product"] = conf["product"]
        kwargs["revision"] = conf["revision"]
        kwargs["block_size"] = conf["block_size"]
        expected_block_size = conf["block_size"]

    process = attach_image(scsi_id, **kwargs)
    if process["status"]:
        flash(f"Attached {file_name} to SCSI ID {scsi_id} LUN {unit}")
        if int(file_size) % int(expected_block_size):
            flash(f"The image file size {file_size} bytes is not a multiple of "
                  f"{expected_block_size} and RaSCSI will ignore the trailing data. "
                  f"The image may be corrupted so proceed with caution.", "error")
        return content_aware_redirect("index")

    flash(f"Failed to attach {file_name} to SCSI ID {scsi_id} LUN {unit}", "error")
    flash(process["msg"], "error")
    return content_aware_redirect("index")


@APP.route("/scsi/detach_all", methods=["POST"])
def detach_all_devices():
    """
    Detaches all currently attached devices
    """
    process = detach_all()
    if process["status"]:
        flash("Detached all SCSI devices")
        flash("A second message class flash")
        flash("oh an error too!", "error")
        return content_aware_redirect("index")

    flash(process["msg"], "error")
    return content_aware_redirect("index")


@APP.route("/scsi/detach", methods=["POST"])
def detach():
    """
    Detaches a specified device
    """
    scsi_id = get_param("scsi_id")
    unit = get_param("unit")
    process = detach_by_id(scsi_id, unit)
    if process["status"]:
        flash(f"Detached SCSI ID {scsi_id} LUN {unit}")
        return content_aware_redirect("index")

    flash(f"Failed to detach SCSI ID {scsi_id} LUN {unit}", "error")
    flash(process["msg"], "error")
    return content_aware_redirect("index")


@APP.route("/scsi/eject", methods=["POST"])
def eject():
    """
    Ejects a specified removable device image, but keeps the device attached
    """
    scsi_id = get_param("scsi_id")
    unit = get_param("unit")

    process = eject_by_id(scsi_id, unit)
    if process["status"]:
        flash(f"Ejected SCSI ID {scsi_id} LUN {unit}")
        return content_aware_redirect("index")

    flash(f"Failed to eject SCSI ID {scsi_id} LUN {unit}", "error")
    flash(process["msg"], "error")
    return content_aware_redirect("index")

@APP.route("/scsi/info", methods=["POST"])
def device_info():
    """
    Displays detailed info for a specific device
    """
    scsi_id = get_param("scsi_id")
    unit = get_param("unit")

    devices = list_devices(scsi_id, unit)

    # First check if any device at all was returned
    if not devices["status"]:
        flash(devices["msg"], "error")
        return content_aware_redirect("index")
    # Looking at the first dict in list to get
    # the one and only device that should have been returned
    device = devices["device_list"][0]
    if str(device["id"]) == scsi_id:
        flash("=== DEVICE INFO ===")
        flash(f"SCSI ID: {device['id']}")
        flash(f"LUN: {device['unit']}")
        flash(f"Type: {device['device_type']}")
        flash(f"Status: {device['status']}")
        flash(f"File: {device['image']}")
        flash(f"Parameters: {device['params']}")
        flash(f"Vendor: {device['vendor']}")
        flash(f"Product: {device['product']}")
        flash(f"Revision: {device['revision']}")
        flash(f"Block Size: {device['block_size']} bytes")
        flash(f"Image Size: {device['size']} bytes")
        return content_aware_redirect("index")

    flash(devices["msg"], "error")
    return content_aware_redirect("index")

@APP.route("/scsi/reserve", methods=["POST"])
def reserve_id():
    """
    Reserves a SCSI ID and stores the memo for that reservation
    """
    scsi_id = get_param("scsi_id")
    memo = get_param("memo")
    reserved_ids = get_reserved_ids()["ids"]
    reserved_ids.extend(scsi_id)
    process = reserve_scsi_ids(reserved_ids)
    if process["status"]:
        RESERVATIONS[int(scsi_id)] = memo
        flash(f"Reserved SCSI ID {scsi_id}")
        return content_aware_redirect("index")

    flash(process["msg"], "error")
    return content_aware_redirect("index")

@APP.route("/scsi/unreserve", methods=["POST"])
def unreserve_id():
    """
    Removes the reservation of a SCSI ID as well as the memo for the reservation
    """
    scsi_id = get_param("scsi_id")
    reserved_ids = get_reserved_ids()["ids"]
    reserved_ids.remove(scsi_id)
    process = reserve_scsi_ids(reserved_ids)
    if process["status"]:
        RESERVATIONS[int(scsi_id)] = ""
        flash(f"Released the reservation for SCSI ID {scsi_id}")
        return content_aware_redirect("index")

    flash(process["msg"], "error")
    return content_aware_redirect("index")


@APP.route("/pi/reboot", methods=["POST"])
def restart():
    """
    Restarts the Pi
    """
    detach_all()
    flash("Safely detached all devices.")
    flash("Rebooting the Pi momentarily...")
    reboot_pi()
    return content_aware_redirect("index")


@APP.route("/rascsi/restart", methods=["POST"])
def rascsi_restart():
    """
    Restarts the RaSCSI backend service
    """
    detach_all()
    flash("Safely detached all devices.")
    flash("Restarting RaSCSI Service...")
    systemd_service("rascsi.service", "restart")
    systemd_service("monitor_rascsi.service", "restart")
    return content_aware_redirect("index")


@APP.route("/pi/shutdown", methods=["POST"])
def shutdown():
    """
    Shuts down the Pi
    """
    detach_all()
    flash("Safely detached all devices.")
    flash("Shutting down the Pi momentarily...")
    shutdown_pi()
    return content_aware_redirect("index")


@APP.route("/files/download_to_iso", methods=["POST"])
def download_to_iso():
    """
    Downloads a remote file and creates a CD-ROM image formatted with HFS that contains the file
    """
    scsi_id = get_param("scsi_id")
    url = get_param("url")

    process = download_file_to_iso(url)
    if process["status"]:
        flash(f"Created CD-ROM image: {process['file_name']}")
    else:
        flash(f"Failed to create CD-ROM image from {url}", "error")
        flash(process["msg"], "error")
        return content_aware_redirect("index")

    process_attach = attach_image(scsi_id, device_type="SCCD", image=process["file_name"])
    if process_attach["status"]:
        flash(f"Attached to SCSI ID {scsi_id}")
        return content_aware_redirect("index")

    flash(f"Failed to attach image to SCSI ID {scsi_id}. Try attaching it manually.", "error")
    flash(process_attach["msg"], "error")
    return content_aware_redirect("index")


@APP.route("/files/download_to_images", methods=["POST"])
def download_img():
    """
    Downloads a remote file onto the images dir on the Pi
    """
    url = get_param("url")
    server_info = get_server_info()
    process = download_to_dir(url, server_info["image_dir"])
    if process["status"]:
        flash(process["msg"])
        return content_aware_redirect("index")

    flash(f"Failed to download file {url}", "error")
    flash(process["msg"], "error")
    return content_aware_redirect("index")


@APP.route("/files/download_to_afp", methods=["POST"])
def download_afp():
    """
    Downloads a remote file onto the AFP shared dir on the Pi
    """
    url = get_param("url")
    process = download_to_dir(url, AFP_DIR)
    if process["status"]:
        flash(process["msg"])
        return content_aware_redirect("index")

    flash(f"Failed to download file {url}", "error")
    flash(process["msg"], "error")
    return content_aware_redirect("index")


@APP.route("/files/upload", methods=["POST"])
def upload_file():
    """
    Uploads a file from the local computer to the images dir on the Pi
    Depending on the Dropzone.js JavaScript library
    """
    from werkzeug.utils import secure_filename
    from os import path

    log = logging.getLogger("pydrop")
    file = request.files["file"]
    filename = secure_filename(file.filename)

    server_info = get_server_info()

    save_path = path.join(server_info["image_dir"], filename)
    current_chunk = int(request.form['dzchunkindex'])

    # Makes sure not to overwrite an existing file,
    # but continues writing to a file transfer in progress
    if path.exists(save_path) and current_chunk == 0:
        return make_response((f"The file {file.filename} already exists!", 400))

    try:
        with open(save_path, "ab") as save:
            save.seek(int(request.form["dzchunkbyteoffset"]))
            save.write(file.stream.read())
    except OSError:
        log.exception("Could not write to file")
        return make_response(("Unable to write the file to disk!", 500))

    total_chunks = int(request.form["dztotalchunkcount"])

    if current_chunk + 1 == total_chunks:
        # Validate the resulting file size after writing the last chunk
        if path.getsize(save_path) != int(request.form["dztotalfilesize"]):
            log.error("Finished transferring %s, "
                      "but it has a size mismatch with the original file."
                      "Got %s but we expected %s.",
                      file.filename, path.getsize(save_path), request.form['dztotalfilesize'])
            return make_response(("Transferred file corrupted!", 500))

        log.info("File %s has been uploaded successfully", file.filename)
    log.debug("Chunk %s of %s for file %s completed.",
              current_chunk + 1, total_chunks, file.filename)

    return make_response(("File upload successful!", 200))


@APP.route("/files/create", methods=["POST"])
def create_file():
    """
    Creates an empty image file in the images dir
    """
    file_name = get_param("file_name")
    size = (get_param("size", 0, int) * 1024 * 1024)
    file_type = get_param("type")

    from werkzeug.utils import secure_filename
    file_name = secure_filename(file_name)

    process = create_new_image(file_name, file_type, size)
    if process["status"]:
        flash(f"Drive image created: {file_name}.{file_type}")
        return content_aware_redirect("index")

    flash(process["msg"], "error")
    return content_aware_redirect("index")


@APP.route("/files/download", methods=["GET"])
def download():
    """
    Downloads a file from the Pi to the local computer
    """
    image = request.args.get('image')
    server_info = get_server_info()
    return send_file(f"{server_info['image_dir']}/{image}", as_attachment=True)


@APP.route("/files/delete", methods=["POST"])
def delete():
    """
    Deletes a specified file in the images dir
    """
    file_name = get_param("image")

    process = delete_image(file_name)
    if process["status"]:
        flash(f"Image file deleted: {file_name}")
    else:
        flash(process["msg"], "error")
        return content_aware_redirect("index")

    # Delete the drive properties file, if it exists
    prop_file_path = f"{CFG_DIR}{file_name}.{PROPERTIES_SUFFIX}"
    if Path(prop_file_path).is_file():
        process = delete_file(prop_file_path)
        if process["status"]:
            flash(process["msg"])
            return content_aware_redirect("index")

        flash(process["msg"], "error")
        return content_aware_redirect("index")

    return content_aware_redirect("index")


@APP.route("/files/unzip", methods=["POST"])
def unzip():
    """
    Unzips a specified zip file
    """
    image = get_param("image")
    member = get_param("member", False)

    process = unzip_file(image, member)
    if process["status"]:
        if not process["msg"]:
            flash("Aborted unzip: File(s) with the same name already exists.", "error")
            return content_aware_redirect("index")
        flash("Unzipped the following files:")
        for msg in process["msg"]:
            flash(msg)
        return content_aware_redirect("index")

    flash("Failed to unzip " + image, "error")
    flash(process["msg"], "error")
    return content_aware_redirect("index")


def get_param(param, default=None, cast_to=None):
    """
    Get a param from the form or json object. If both exist, the form takes precedence.
    Matches API of request.form.get and request.json.get.
    :param param: name of the parameter to get
    :param default: default if it doesnt exist.
    :param cast_to: type of the parameter to be cast to.
    :return: the value of the parameter or default, typed.
    """
    rv = request.form.get(param, request.json.get(param, default))

    if rv is not None and cast_to is not None:
        try:
            rv = cast_to(rv)
        except ValueError:
            rv = default
    return rv


def is_json(req):
    """
    Checks if the request should be served back json or html
    :param req: http request
    :return: boolean
    """
    return req.headers.get('Accept').lower() in 'application/json'


def content_aware_redirect(url):
    """
    Renders a redirect or a json response based on the request's Accept header
    :param url:
    :return:
    """
    if is_json(request):
        return jsonify({"flash": _flashed_messages_to_json(), "redirect": url})
    else:
        return redirect(url_for(url))


def content_aware_response(template, **kwargs):
    """
    Renders a template or a json response based on the request's Accept header
    :param template: name of the template to render
    :param kwargs: data to render to the template or json object.
    :return:
    """
    if is_json(request):
        json = _protobuf_to_python(kwargs)
        return jsonify({"flash": _flashed_messages_to_json(), "template": template, "data": json})
    else:
        return render_template(template, **kwargs)


def _flashed_messages_to_json():
    """
    Converts the flashed messages to a json list with categories.
    :return: json list of messages
    """
    flashed = get_flashed_messages(with_categories=True)
    messages = {"error": [], "message": []}
    for category, message in flashed:
        if category not in messages:
            messages[category] = []
        messages[category].append(message)
    return messages


def _protobuf_to_python(message):
    """
    Converts a dict with Protobuf messages into a dict that can be serialized to json.
    :param message: A mixed dict with Protobuf messages
    :return: dict with no Protobuf types
    """
    json = {}
    if not isinstance(message, dict) and not isinstance(message, list) and not isinstance(message, RepeatedScalarContainer) and not isinstance(message, ScalarMapContainer):
        # Not something we are interested in converting.
        return message
    for key, value in message.items():
        new_value = value
        if isinstance(value, dict):
            new_value = _protobuf_to_python(value)
        if isinstance(value, list):
            new_value = [_protobuf_to_python(item) for item in value]
        if isinstance(value, RepeatedScalarContainer):
            new_value = list(value)
        if isinstance(value, ScalarMapContainer):
            new_value = dict(value)
        json[key] = new_value
    return json


if __name__ == "__main__":
    APP.secret_key = "rascsi_is_awesome_insecure_secret_key"
    APP.config["SESSION_TYPE"] = "filesystem"
    APP.config["MAX_CONTENT_LENGTH"] = int(MAX_FILE_SIZE)

    # Load the default configuration file, if found
    if Path(CFG_DIR + DEFAULT_CONFIG).is_file():
        read_config(DEFAULT_CONFIG)

    import bjoern
    print("Serving rascsi-web...")
    bjoern.run(APP, "0.0.0.0", 8080)
