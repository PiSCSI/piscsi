from flask import Flask, render_template, request, flash, url_for, redirect, send_file, send_from_directory

from file_cmds import (
    list_files,
    list_config_files,
    create_new_image,
    download_file_to_iso,
    delete_file,
    unzip_file,
    download_image,
    write_config,
    read_config,
    read_device_config,
)
from pi_cmds import (
    shutdown_pi, 
    reboot_pi, 
    running_version, 
    rascsi_service,
    is_bridge_setup,
)
from ractl_cmds import (
    attach_image,
    list_devices,
    sort_and_format_devices,
    detach_by_id,
    eject_by_id,
    get_valid_scsi_ids,
    attach_daynaport,
    detach_all,
    reserve_scsi_ids,
    get_server_info,
    validate_scsi_id,
    set_log_level,
)
from settings import *

app = Flask(__name__)


@app.route("/")
def index():
    reserved_scsi_ids = app.config.get("RESERVED_SCSI_IDS")
    unsorted_devices, occupied_ids = list_devices()
    devices = sort_and_format_devices(unsorted_devices, occupied_ids)
    scsi_ids = get_valid_scsi_ids(devices, list(reserved_scsi_ids), occupied_ids)
    return render_template(
        "index.html",
        bridge_configured=is_bridge_setup(),
        devices=devices,
        files=list_files(),
        config_files=list_config_files(),
        base_dir=base_dir,
        scsi_ids=scsi_ids,
        reserved_scsi_ids=[reserved_scsi_ids],
        max_file_size=MAX_FILE_SIZE,
        version=running_version(),
        server_info=get_server_info(),
        valid_file_suffix=VALID_FILE_SUFFIX,
        removable_device_types=REMOVABLE_DEVICE_TYPES,
        harddrive_file_suffix=HARDDRIVE_FILE_SUFFIX,
        cdrom_file_suffix=CDROM_FILE_SUFFIX,
        removable_file_suffix=REMOVABLE_FILE_SUFFIX,
        archive_file_suffix=ARCHIVE_FILE_SUFFIX,
    )

@app.route('/pwa/<path:path>')
def send_pwa_files(path):
    return send_from_directory('pwa', path)

@app.route("/config/save", methods=["POST"])
def config_save():
    file_name = request.form.get("name") or "default"
    file_name = f"{base_dir}{file_name}.json"

    process = write_config(file_name)
    if process["status"] == True:
        flash(f"Saved config to  {file_name}!")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to saved config to  {file_name}!", "error")
        flash(f"{process['msg']}", "error")
    return redirect(url_for("index"))


@app.route("/config/load", methods=["POST"])
def config_load():
    file_name = request.form.get("name")
    file_name = f"{base_dir}{file_name}"

    if "load" in request.form:
        process = read_config(file_name)
        if process["status"] == True:
            flash(f"Loaded config from  {file_name}!")
        else:
            flash(f"Failed to load  {file_name}!", "error")
            flash(f"{process['msg']}", "error")
    elif "delete" in request.form:
        if delete_file(file_name):
            flash(f"Deleted config  {file_name}!")
        else:
            flash(f"Failed to delete  {file_name}!", "error")

    return redirect(url_for("index"))


@app.route("/logs/show", methods=["POST"])
def show_logs():
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
    else:
        flash("Failed to get logs")
        flash(process.stdout.decode("utf-8"), "stdout")
        flash(process.stderr.decode("utf-8"), "stderr")
        return redirect(url_for("index"))


@app.route("/logs/level", methods=["POST"])
def log_level():
    level = request.form.get("level") or "info"

    process = set_log_level(level)
    if process["status"] == True:
        flash(f"Log level set to {level}!")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to set log level to {level}!", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))


@app.route("/daynaport/attach", methods=["POST"])
def daynaport_attach():
    scsi_id = request.form.get("scsi_id")

    validate = validate_scsi_id(scsi_id)
    if validate["status"] == False:
        flash(validate["msg"], "error")
        return redirect(url_for("index"))

    process = attach_daynaport(scsi_id)
    if process["status"] == True:
        flash(f"Attached DaynaPORT to SCSI id {scsi_id}!")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to attach DaynaPORT to SCSI id {scsi_id}!", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))


@app.route("/scsi/attach", methods=["POST"])
def attach():
    file_name = request.form.get("file_name")
    file_size = request.form.get("file_size")
    scsi_id = request.form.get("scsi_id")

    validate = validate_scsi_id(scsi_id)
    if validate["status"] == False:
        flash(validate["msg"], "error")
        return redirect(url_for("index"))

    kwargs = {"image": file_name}

    # Attempt to load the device config sidecar file:
    # same base path but .rascsi instead of the original suffix.
    from pathlib import Path
    device_config = Path(base_dir + str(Path(file_name).stem) + ".rascsi")
    if device_config.is_file():
        process = read_device_config(device_config)
        if process["status"] == False:
            flash(process["msg"], "error")
            return redirect(url_for("index"))
        conf = process["conf"]
        conf_file_size = conf["blocks"] * conf["block_size"]
        if conf_file_size != 0 and conf_file_size > int(file_size):
            flash(f"Failed to attach {file_name} to SCSI id {scsi_id}!", "error")
            flash(f"The file size {file_size} bytes needs to be at least {conf_file_size} bytes.", "error")
            return redirect(url_for("index"))
        kwargs["device_type"] = conf["device_type"]
        kwargs["vendor"] = conf["vendor"]
        kwargs["product"] = conf["product"]
        kwargs["revision"] = conf["revision"]
        kwargs["block_size"] = conf["block_size"]
    # Validate image type by file name suffix as fallback
    elif file_name.lower().endswith(CDROM_FILE_SUFFIX):
        kwargs["device_type"] = "SCCD"
    elif file_name.lower().endswith(REMOVABLE_FILE_SUFFIX):
        kwargs["device_type"] = "SCRM"
    elif file_name.lower().endswith(HARDDRIVE_FILE_SUFFIX):
        kwargs["device_type"] = "SCHD"
 
    process = attach_image(scsi_id, **kwargs)
    if process["status"] == True:
        flash(f"Attached {file_name} to SCSI id {scsi_id}!")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to attach {file_name} to SCSI id {scsi_id}!", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))


@app.route("/scsi/detach_all", methods=["POST"])
def detach_all_devices():
    process = detach_all()
    if process["status"] == True:
        flash("Detached all SCSI devices!")
        return redirect(url_for("index"))
    else:
        flash("Failed to detach all SCSI devices!", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))


@app.route("/scsi/detach", methods=["POST"])
def detach():
    scsi_id = request.form.get("scsi_id")
    process = detach_by_id(scsi_id)
    if process["status"] == True:
        flash(f"Detached SCSI id {scsi_id}!")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to detach SCSI id {scsi_id}!", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))


@app.route("/scsi/eject", methods=["POST"])
def eject():
    scsi_id = request.form.get("scsi_id")
    process = eject_by_id(scsi_id)
    if process["status"] == True:
        flash(f"Ejected scsi id {scsi_id}!")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to eject SCSI id {scsi_id}!", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))

@app.route("/scsi/info", methods=["POST"])
def device_info():
    scsi_id = request.form.get("scsi_id")
    # Extracting the 0th dictionary in list index 0
    device = list_devices(scsi_id)[0][0]
    if str(device["id"]) == scsi_id:
        flash("=== DEVICE INFO ===")
        flash(f"SCSI ID: {device['id']}")
        flash(f"Unit: {device['un']}")
        flash(f"Type: {device['device_type']}")
        flash(f"Status: {device['status']}")
        flash(f"File: {device['image']}")
        flash(f"Parameters: {device['params']}")
        flash(f"Vendor: {device['vendor']}")
        flash(f"Product: {device['product']}")
        flash(f"Revision: {device['revision']}")
        flash(f"Block Size: {device['block_size']}")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to get device info for SCSI id {scsi_id}!", "error")
        return redirect(url_for("index"))

@app.route("/pi/reboot", methods=["POST"])
def restart():
    reboot_pi()
    flash("Restarting...")
    return redirect(url_for("index"))


@app.route("/rascsi/restart", methods=["POST"])
def rascsi_restart():
    rascsi_service("restart")
    flash("Restarting RaSCSI Service...")
    reserved_scsi_ids = app.config.get("RESERVED_SCSI_IDS") 
    if reserved_scsi_ids != "":
        reserve_scsi_ids(reserved_scsi_ids)
    return redirect(url_for("index"))


@app.route("/pi/shutdown", methods=["POST"])
def shutdown():
    shutdown_pi()
    flash("Shutting down...")
    return redirect(url_for("index"))


@app.route("/files/download_to_iso", methods=["POST"])
def download_file():
    scsi_id = request.form.get("scsi_id")
    url = request.form.get("url")
    process = download_file_to_iso(scsi_id, url)
    if process["status"] == True:
        flash(f"File Downloaded and Attached to SCSI id {scsi_id}")
        flash(process["msg"])
        return redirect(url_for("index"))
    else:
        flash(f"Failed to download and attach file {url}", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))


@app.route("/files/download_image", methods=["POST"])
def download_img():
    url = request.form.get("url")
    process = download_image(url)
    if process["status"] == True:
        flash(f"File Downloaded from {url}")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to download file {url}", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))


@app.route("/files/upload/<filename>", methods=["POST"])
def upload_file(filename):
    if not filename:
        flash("No file provided.", "error")
        return redirect(url_for("index"))

    from os import path
    file_path = path.join(app.config["UPLOAD_FOLDER"], filename)
    if path.isfile(file_path):
        flash(f"{filename} already exists.", "error")
        return redirect(url_for("index"))

    from io import DEFAULT_BUFFER_SIZE
    binary_new_file = "bx"
    with open(file_path, binary_new_file, buffering=DEFAULT_BUFFER_SIZE) as f:
        chunk_size = DEFAULT_BUFFER_SIZE
        while True:
            chunk = request.stream.read(chunk_size)
            if len(chunk) == 0:
                break
            f.write(chunk)
    # TODO: display an informative success message
    return redirect(url_for("index", filename=filename))


@app.route("/files/create", methods=["POST"])
def create_file():
    file_name = request.form.get("file_name")
    size = request.form.get("size")
    file_type = request.form.get("type")

    process = create_new_image(file_name, file_type, size)
    if process.returncode == 0:
        flash("Drive created")
        return redirect(url_for("index"))
    else:
        flash("Failed to create file", "error")
        flash(process.stdout, "stdout")
        flash(process.stderr, "stderr")
        return redirect(url_for("index"))


@app.route("/files/download", methods=["POST"])
def download():
    image = request.form.get("image")
    return send_file(base_dir + image, as_attachment=True)


@app.route("/files/delete", methods=["POST"])
def delete():
    image = request.form.get("image")
    if delete_file(base_dir + image):
        flash("File " + image + " deleted")
        return redirect(url_for("index"))
    else:
        flash("Failed to Delete " + image, "error")
        return redirect(url_for("index"))


@app.route("/files/unzip", methods=["POST"])
def unzip():
    image = request.form.get("image")

    if unzip_file(image):
        flash("Unzipped file " + image)
        return redirect(url_for("index"))
    else:
        flash("Failed to unzip " + image, "error")
        return redirect(url_for("index"))


if __name__ == "__main__":
    app.secret_key = "rascsi_is_awesome_insecure_secret_key"
    app.config["SESSION_TYPE"] = "filesystem"
    app.config["UPLOAD_FOLDER"] = base_dir

    from os import makedirs
    makedirs(app.config["UPLOAD_FOLDER"], exist_ok=True)
    app.config["MAX_CONTENT_LENGTH"] = MAX_FILE_SIZE

    from sys import argv
    if len(argv) >= 2:
        # Reserved ids format is a string of digits such as '017'
        app.config["RESERVED_SCSI_IDS"] = str(argv[1])
        # Reserve SCSI IDs on the backend side to prevent use
        reserve_scsi_ids(app.config.get("RESERVED_SCSI_IDS"))
    else:
        app.config["RESERVED_SCSI_IDS"] = ""

    # Load the default configuration file, if found
    from pathlib import Path
    default_config = Path(DEFAULT_CONFIG)
    if default_config.is_file():
        read_config(default_config)

    import bjoern
    print("Serving rascsi-web...")
    bjoern.run(app, "0.0.0.0", 8080)

