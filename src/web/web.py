import logging
from pathlib import Path

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
    get_network_info,
    get_device_types,
    set_log_level,
)
from device_utils import (
    sort_and_format_devices,
    get_valid_scsi_ids,
)
from settings import *

app = Flask(__name__)


@app.route("/")
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
    luns = 0
    for d in devices["device_list"]:
        attached_images.append(Path(d["image"]).name)
        luns += int(d["un"])

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

    return render_template(
        "index.html",
        bridge_configured=is_bridge_setup(),
        netatalk_configured=running_netatalk(),
        devices=formatted_devices,
        files=sorted_image_files,
        config_files=sorted_config_files,
        base_dir=server_info["image_dir"],
        cfg_dir=cfg_dir,
        afp_dir=afp_dir,
        scsi_ids=scsi_ids,
        recommended_id=recommended_id,
        attached_images=attached_images,
        luns=luns,
        reserved_scsi_ids=reserved_scsi_ids,
        max_file_size=int(MAX_FILE_SIZE / 1024 / 1024),
        running_env=running_env(),
        version=server_info["version"],
        log_levels=server_info["log_levels"],
        current_log_level=server_info["current_log_level"],
        netinfo=get_network_info(),
        device_types=device_types["device_types"],
        free_disk=int(disk["free"] / 1024 / 1024),
        valid_file_suffix=valid_file_suffix,
        archive_file_suffix=ARCHIVE_FILE_SUFFIX,
        removable_device_types=REMOVABLE_DEVICE_TYPES,
    )


@app.route("/drive/list", methods=["GET"])
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
            return redirect(url_for("index"))
        conf = process["conf"]
    else:
        flash("Could not read drive properties from " + str(drive_properties), "error")
        return redirect(url_for("index"))

    hd_conf = []
    cd_conf = []
    rm_conf = []

    from werkzeug.utils import secure_filename
    for d in conf:
        if d["device_type"] == "SCHD":
            d["secure_name"] = secure_filename(d["name"])
            d["size_mb"] = "{:,.2f}".format(d["size"] / 1024 / 1024)
            hd_conf.append(d)
        elif d["device_type"] == "SCCD":
            d["size_mb"] = "N/A"
            cd_conf.append(d)
        elif d["device_type"] == "SCRM":
            d["secure_name"] = secure_filename(d["name"])
            d["size_mb"] = "{:,.2f}".format(d["size"] / 1024 / 1024)
            rm_conf.append(d)

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


@app.route('/pwa/<path:path>')
def send_pwa_files(path):
    """
    Sets up mobile web resources
    """
    return send_from_directory('pwa', path)


@app.route("/drive/create", methods=["POST"])
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

    # Creating the image file
    process = create_new_image(file_name, file_type, size)
    if process["status"]:
        flash(f"Drive image file {file_name}.{file_type} created")
        # TODO Revisit after resolving issue#410 (more instances below)
        flash(process["msg"])
    else:
        flash(f"Failed to create file {file_name}.{file_type}", "error")
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
    process = write_drive_properties(prop_file_name, properties)
    if process["status"]:
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(process['msg'], "error")
    return redirect(url_for("index"))


@app.route("/drive/cdrom", methods=["POST"])
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
    process = write_drive_properties(file_name, properties)
    if process["status"]:
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(process['msg'], "error")
    return redirect(url_for("index"))


@app.route("/config/save", methods=["POST"])
def config_save():
    """
    Saves a config file to disk
    """
    file_name = request.form.get("name") or "default"
    file_name = f"{file_name}.{CONFIG_FILE_SUFFIX}"

    process = write_config(file_name)
    if process["status"]:
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(process['msg'], "error")
    return redirect(url_for("index"))


@app.route("/config/load", methods=["POST"])
def config_load():
    """
    Loads a config file from disk
    """
    file_name = request.form.get("name")

    if "load" in request.form:
        process = read_config(file_name)
        if process["status"]:
            flash(process["msg"])
            return redirect(url_for("index"))

        flash(process['msg'], "error")
        return redirect(url_for("index"))
    elif "delete" in request.form:
        process = delete_file(cfg_dir + file_name)
        if process["status"]:
            flash(process["msg"])
            return redirect(url_for("index"))

        flash(process['msg'], "error")
        return redirect(url_for("index"))

    flash("Got an unhandled request (needs to be either load or delete)", "error")
    return redirect(url_for("index"))


@app.route("/logs/show", methods=["POST"])
def show_logs():
    """
    Displays system logs
    """
    lines = request.form.get("lines") or "200"
    scope = request.form.get("scope") or "default"

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
    return redirect(url_for("index"))


@app.route("/logs/level", methods=["POST"])
def log_level():
    """
    Sets RaSCSI backend log level
    """
    level = request.form.get("level") or "info"

    process = set_log_level(level)
    if process["status"]:
        flash(f"Log level set to {level}!")
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(f"Failed to set log level to {level}!", "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@app.route("/daynaport/attach", methods=["POST"])
def daynaport_attach():
    """
    Attaches a DaynaPORT ethernet adapter device
    """
    scsi_id = request.form.get("scsi_id")
    interface = request.form.get("if")
    ip = request.form.get("ip")
    mask = request.form.get("mask")

    kwargs = {"device_type": "SCDP"}
    if interface != "":
        arg = interface
        if "" not in (ip, mask):
            arg += (":" + ip + "/" + mask)
        kwargs["interfaces"] = arg

    process = attach_image(scsi_id, **kwargs)
    if process["status"]:
        flash(f"Attached DaynaPORT to SCSI ID {scsi_id}!")
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(process["msg"], "error")
    return redirect(url_for("index"))


@app.route("/scsi/attach", methods=["POST"])
def attach():
    """
    Attaches a file image as a device
    """
    file_name = request.form.get("file_name")
    file_size = request.form.get("file_size")
    scsi_id = request.form.get("scsi_id")
    un = request.form.get("un")
    device_type = request.form.get("type")

    kwargs = {"unit": int(un), "image": file_name}

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
    drive_properties = f"{cfg_dir}{file_name}.{PROPERTIES_SUFFIX}"
    if Path(drive_properties).is_file():
        process = read_drive_properties(drive_properties)
        if not process["status"]:
            flash("Failed to load the device properties file " + \
                  file_name + "." + PROPERTIES_SUFFIX, "error")
            flash(process["msg"], "error")
            return redirect(url_for("index"))
        conf = process["conf"]
        kwargs["vendor"] = conf["vendor"]
        kwargs["product"] = conf["product"]
        kwargs["revision"] = conf["revision"]
        kwargs["block_size"] = conf["block_size"]
        expected_block_size = conf["block_size"]

    process = attach_image(scsi_id, **kwargs)
    if process["status"]:
        flash(f"Attached {file_name} to SCSI ID {scsi_id} LUN {un}!")
        flash(process["msg"])
        if int(file_size) % int(expected_block_size):
            flash(f"The image file size {file_size} bytes is not a multiple of \
                    {expected_block_size} and RaSCSI will ignore the trailing data. \
                    The image may be corrupted so proceed with caution.", "error")
        return redirect(url_for("index"))

    flash(f"Failed to attach {file_name} to SCSI ID {scsi_id} LUN {un}!", "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@app.route("/scsi/detach_all", methods=["POST"])
def detach_all_devices():
    """
    Detaches all currently attached devices
    """
    process = detach_all()
    if process["status"]:
        flash("Detached all SCSI devices!")
        flash(process["msg"])
        return redirect(url_for("index"))

    flash("Failed to detach all SCSI devices!", "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@app.route("/scsi/detach", methods=["POST"])
def detach():
    """
    Detaches a specified device
    """
    scsi_id = request.form.get("scsi_id")
    un = request.form.get("un")
    process = detach_by_id(scsi_id, un)
    if process["status"]:
        flash(f"Detached SCSI ID {scsi_id} LUN {un}!")
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(f"Failed to detach SCSI ID {scsi_id} LUN {un}!", "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@app.route("/scsi/eject", methods=["POST"])
def eject():
    """
    Ejects a specified removable device image, but keeps the device attached
    """
    scsi_id = request.form.get("scsi_id")
    un = request.form.get("un")

    process = eject_by_id(scsi_id, un)
    if process["status"]:
        flash(f"Ejected SCSI ID {scsi_id} LUN {un}!")
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(f"Failed to eject SCSI ID {scsi_id} LUN {un}!", "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))

@app.route("/scsi/info", methods=["POST"])
def device_info():
    """
    Displays detailed info for a specific device
    """
    scsi_id = request.form.get("scsi_id")
    un = request.form.get("un")

    devices = list_devices(scsi_id, un)

    # First check if any device at all was returned
    if not devices["status"]:
        flash(f"No device attached to SCSI ID {scsi_id} LUN {un}!", "error")
        flash(devices["msg"], "error")
        return redirect(url_for("index"))
    # Looking at the first dict in list to get
    # the one and only device that should have been returned
    device = devices["device_list"][0]
    if str(device["id"]) == scsi_id:
        flash("=== DEVICE INFO ===")
        flash(f"SCSI ID: {device['id']}")
        flash(f"LUN: {device['un']}")
        flash(f"Type: {device['device_type']}")
        flash(f"Status: {device['status']}")
        flash(f"File: {device['image']}")
        flash(f"Parameters: {device['params']}")
        flash(f"Vendor: {device['vendor']}")
        flash(f"Product: {device['product']}")
        flash(f"Revision: {device['revision']}")
        flash(f"Block Size: {device['block_size']} bytes")
        flash(f"Image Size: {device['size']} bytes")
        return redirect(url_for("index"))

    flash(f"Failed to get device info for SCSI ID {scsi_id} LUN {un}!", "error")
    flash(devices["msg"], "error")
    return redirect(url_for("index"))

@app.route("/pi/reboot", methods=["POST"])
def restart():
    """
    Restarts the Pi
    """
    detach_all()
    flash("Safely detached all devices.")
    flash("Rebooting the Pi momentarily...")
    reboot_pi()
    return redirect(url_for("index"))


@app.route("/rascsi/restart", methods=["POST"])
def rascsi_restart():
    """
    Restarts the RaSCSI backend service
    """
    detach_all()
    flash("Safely detached all devices.")
    flash("Restarting RaSCSI Service...")
    systemd_service("rascsi.service", "restart")
    systemd_service("monitor_rascsi.service", "restart")
    return redirect(url_for("index"))


@app.route("/pi/shutdown", methods=["POST"])
def shutdown():
    """
    Shuts down the Pi
    """
    detach_all()
    flash("Safely detached all devices.")
    flash("Shutting down the Pi momentarily...")
    shutdown_pi()
    return redirect(url_for("index"))


@app.route("/files/download_to_iso", methods=["POST"])
def download_to_iso():
    """
    Downloads a remote file and creates a CD-ROM image formatted with HFS that contains the file
    """
    scsi_id = request.form.get("scsi_id")
    url = request.form.get("url")
 
    process = download_file_to_iso(url)
    if process["status"]:
        flash(f"Created CD-ROM image {process['file_name']}")
        flash(process["msg"])
    else:
        flash(f"Failed to create CD-ROM image from {url}", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))

    process_attach = attach_image(scsi_id, device_type="SCCD", image=process["file_name"])
    if process_attach["status"]:
        flash(f"Attached to SCSI ID {scsi_id}")
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(f"Failed to attach to SCSI ID {scsi_id}. Try attaching it manually.", "error")
    flash(process_attach["msg"], "error")
    return redirect(url_for("index"))


@app.route("/files/download_to_images", methods=["POST"])
def download_img():
    """
    Downloads a remote file onto the images dir on the Pi
    """
    url = request.form.get("url")
    server_info = get_server_info()
    process = download_to_dir(url, server_info["image_dir"])
    if process["status"]:
        flash(f"File Downloaded from {url} to {server_info['image_dir']}")
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(f"Failed to download file {url}", "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@app.route("/files/download_to_afp", methods=["POST"])
def download_afp():
    """
    Downloads a remote file onto the AFP shared dir on the Pi
    """
    url = request.form.get("url")
    process = download_to_dir(url, afp_dir)
    if process["status"]:
        flash(f"File Downloaded from {url} to {afp_dir}")
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(f"Failed to download file {url}", "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


@app.route("/files/upload", methods=["POST"])
def upload_file():
    """
    Uploads a file from the local computer to the images dir on the Pi
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
        with open(save_path, "ab") as f:
            f.seek(int(request.form["dzchunkbyteoffset"]))
            f.write(file.stream.read())
    except OSError:
        log.exception("Could not write to file")
        return make_response(("Unable to write the file to disk!", 500))

    total_chunks = int(request.form["dztotalchunkcount"])

    if current_chunk + 1 == total_chunks:
        # Validate the resulting file size after writing the last chunk
        if path.getsize(save_path) != int(request.form["dztotalfilesize"]):
            log.error(f"Finished transferring {file.filename}, "
                      f"but it has a size mismatch with the original file."
                      f"Got {path.getsize(save_path)} but we "
                      f"expected {request.form['dztotalfilesize']}.")
            return make_response(("Transferred file corrupted!", 500))

        log.info(f"File {file.filename} has been uploaded successfully")

    log.debug(f"Chunk {current_chunk + 1} of {total_chunks} "
              f"for file {file.filename} completed.")

    return make_response(("File upload successful!", 200))


@app.route("/files/create", methods=["POST"])
def create_file():
    """
    Creates an empty image file in the images dir
    """
    file_name = request.form.get("file_name")
    size = (int(request.form.get("size")) * 1024 * 1024)
    file_type = request.form.get("type")

    from werkzeug.utils import secure_filename
    file_name = secure_filename(file_name)

    process = create_new_image(file_name, file_type, size)
    if process["status"]:
        flash(f"Drive image created as {file_name}.{file_type}")
        flash(process["msg"])
        return redirect(url_for("index"))

    flash(process["msg"], "error")
    return redirect(url_for("index"))


@app.route("/files/download", methods=["POST"])
def download():
    """
    Downloads a file from the Pi to the local computer
    """
    image = request.form.get("image")
    server_info = get_server_info()
    return send_file(f"{server_info['image_dir']}/{image}", as_attachment=True)


@app.route("/files/delete", methods=["POST"])
def delete():
    """
    Deletes a specified file in the images dir
    """
    file_name = request.form.get("image")

    process = delete_image(file_name)
    if process["status"]:
        flash(f"File {file_name} deleted!")
        flash(process["msg"])
    else:
        flash(f"Failed to delete file {file_name}!", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))

    # Delete the drive properties file, if it exists
    prop_file_path = f"{cfg_dir}{file_name}.{PROPERTIES_SUFFIX}"
    if Path(prop_file_path).is_file():
        process = delete_file(prop_file_path)
        if process["status"]:
            flash(f"File {prop_file_path} deleted!")
            flash(process["msg"])
            return redirect(url_for("index"))

        flash(f"Failed to delete file {prop_file_path}!", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))

    return redirect(url_for("index"))


@app.route("/files/unzip", methods=["POST"])
def unzip():
    """
    Unzips a specified zip file
    """
    image = request.form.get("image")
    member = request.form.get("member") or False

    process = unzip_file(image, member)
    if process["status"]:
        if len(process["msg"]) < 1:
            flash("Aborted unzip: File(s) with the same name already exists.", "error")
            return redirect(url_for("index"))
        flash("Unzipped the following files:")
        for m in process["msg"]:
            flash(m)
        return redirect(url_for("index"))

    flash("Failed to unzip " + image, "error")
    flash(process["msg"], "error")
    return redirect(url_for("index"))


if __name__ == "__main__":
    app.secret_key = "rascsi_is_awesome_insecure_secret_key"
    app.config["SESSION_TYPE"] = "filesystem"

    server_info = get_server_info()
    app.config["UPLOAD_FOLDER"] = server_info["image_dir"]

    from os import makedirs
    makedirs(app.config["UPLOAD_FOLDER"], exist_ok=True)
    app.config["MAX_CONTENT_LENGTH"] = MAX_FILE_SIZE

    # Load the default configuration file, if found
    default_config_path = Path(cfg_dir + DEFAULT_CONFIG)
    if default_config_path.is_file():
        read_config(DEFAULT_CONFIG)

    import bjoern
    print("Serving rascsi-web...")
    bjoern.run(app, "0.0.0.0", 8080)
