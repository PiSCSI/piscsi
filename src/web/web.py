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
    write_sidecar,
    read_sidecar,
)
from pi_cmds import (
    shutdown_pi,
    reboot_pi,
    running_env,
    rascsi_service,
    is_bridge_setup,
    net_interfaces,
)
from ractl_cmds import (
    attach_image,
    list_devices,
    sort_and_format_devices,
    detach_by_id,
    eject_by_id,
    get_valid_scsi_ids,
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
    server_info = get_server_info()
    devices = list_devices()

    reserved_scsi_ids = server_info["reserved_ids"]
    formatted_devices = sort_and_format_devices(devices["device_list"])
    scsi_ids = get_valid_scsi_ids(devices["device_list"], reserved_scsi_ids)
    return render_template(
        "index.html",
        bridge_configured=is_bridge_setup(),
        devices=formatted_devices,
        files=list_files(),
        config_files=list_config_files(),
        base_dir=base_dir,
        scsi_ids=scsi_ids,
        reserved_scsi_ids=reserved_scsi_ids,
        max_file_size=MAX_FILE_SIZE,
        running_env=running_env(),
        server_info=server_info,
        ifs = net_interfaces(),
        valid_file_suffix=VALID_FILE_SUFFIX,
        removable_device_types=REMOVABLE_DEVICE_TYPES,
        harddrive_file_suffix=HARDDRIVE_FILE_SUFFIX,
        cdrom_file_suffix=CDROM_FILE_SUFFIX,
        removable_file_suffix=REMOVABLE_FILE_SUFFIX,
        archive_file_suffix=ARCHIVE_FILE_SUFFIX,
    )


@app.route("/disk/list", methods=["GET"])
def disk_list():
    server_info = get_server_info()

    from pathlib import Path
    # TODO: Is this the right place to store sidecars.json?
    sidecar_config = Path(home_dir + "/sidecars.json")
    if sidecar_config.is_file():
        process = read_sidecar(str(sidecar_config))
        if process["status"] == False:
            flash(process["msg"], "error")
            return redirect(url_for("index"))
        conf = process["conf"]
    else:
        flash("Could not read Sidecar configurations " + str(sidecar_config), "error")
        return redirect(url_for("index"))

    hd_conf = []
    cd_conf = []
    rm_conf = []

    for d in conf:
        if d["device_type"] == "SCHD":
            d["size"] = d["block_size"] * d["blocks"]
            d["size_mb"] = "{:,.2f}".format(d["size"] / 1024 / 1024)
            hd_conf.append(d)
        elif d["device_type"] == "SCCD":
            d["size_mb"] = "N/A"
            cd_conf.append(d)
        elif d["device_type"] == "SCRM":
            d["size"] = d["block_size"] * d["blocks"]
            d["size_mb"] = "{:,.2f}".format(d["size"] / 1024 / 1024)
            rm_conf.append(d)

    return render_template(
        "disk.html",
        files=list_files(),
        base_dir=base_dir,
        hd_conf=hd_conf,
        cd_conf=cd_conf,
        rm_conf=rm_conf,
        version=running_version(),
        server_info=server_info,
        cdrom_file_suffix=CDROM_FILE_SUFFIX,
    )


@app.route('/pwa/<path:path>')
def send_pwa_files(path):
    return send_from_directory('pwa', path)


@app.route("/disk/create", methods=["POST"])
def disk_create():
    vendor = request.form.get("vendor")
    product = request.form.get("product")
    revision = request.form.get("revision")
    blocks = request.form.get("blocks")
    block_size = request.form.get("block_size")
    size = request.form.get("size")
    file_type = request.form.get("file_type")
    file_name = request.form.get("file_name")
    
    # Creating the image file
    process = create_new_image(file_name, file_type, size)
    if process["status"] == True:
        flash(f"Drive image file {file_name}.{file_type} created")
        flash(process["msg"])
    else:
        flash(f"Failed to create file {file_name}.{file_type}", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))

    # Creating the sidecar file
    from pathlib import Path
    file_name_base = str(Path(file_name).stem)
    sidecar = {"vendor": vendor, "product": product, "revision": revision, "blocks": blocks, "block_size": block_size}
    process = write_sidecar(base_dir + file_name_base + ".rascsi", sidecar)
    if process["status"] == True:
        flash(f"Drive sidecar file {file_name_base}.rascsi created")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to create sidecar file {file_name_base}.rascsi", "error")
        return redirect(url_for("index"))


@app.route("/disk/cdrom", methods=["POST"])
def disk_cdrom():
    vendor = request.form.get("vendor")
    product = request.form.get("product")
    revision = request.form.get("revision")
    block_size = request.form.get("block_size")
    file_name = request.form.get("file_name")
    from pathlib import Path
    file_name_base = str(Path(file_name).stem)
    
    # Creating the sidecar file
    sidecar = {"vendor": vendor, "product": product, "revision": revision, "block_size": block_size}
    process = write_sidecar(base_dir + file_name_base + ".rascsi", sidecar)
    if process["status"] == True:
        flash(f"Drive sidecar file {file_name_base}.rascsi created")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to create sidecar file {file_name_base}.rascsi", "error")
        return redirect(url_for("index"))


@app.route("/config/save", methods=["POST"])
def config_save():
    file_name = request.form.get("name") or "default"
    file_name = f"{file_name}.json"

    process = write_config(file_name)
    if process["status"] == True:
        flash(f"Saved config to {file_name}!")
        flash(process["msg"])
        return redirect(url_for("index"))
    else:
        flash(f"Failed to saved config to {file_name}!", "error")
        flash(process['msg'], "error")
        return redirect(url_for("index"))


@app.route("/config/load", methods=["POST"])
def config_load():
    file_name = request.form.get("name")

    if "load" in request.form:
        process = read_config(file_name)
        if process["status"] == True:
            flash(f"Loaded config from {file_name}!")
            flash(process["msg"])
            return redirect(url_for("index"))
        else:
            flash(f"Failed to load {file_name}!", "error")
            flash(process['msg'], "error")
            return redirect(url_for("index"))
    elif "delete" in request.form:
        process = delete_file(file_name)
        if process["status"] == True:
            flash(f"Deleted config {file_name}!")
            flash(process["msg"])
            return redirect(url_for("index"))
        else:
            flash(f"Failed to delete {file_name}!", "error")
            flash(process['msg'], "error")
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
    interface = request.form.get("if")
    ip = request.form.get("ip")
    mask = request.form.get("mask")

    kwargs = {"device_type": "SCDP"}
    if interface != "":
        arg = interface
        if "" not in (ip, mask):
            arg += (":" + ip + "/" + mask)
        kwargs["interfaces"] = arg

    validate = validate_scsi_id(scsi_id)
    if validate["status"] == False:
        flash(validate["msg"], "error")
        return redirect(url_for("index"))

    process = attach_image(scsi_id, **kwargs)
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

    # Validate image type by file name suffix
    if file_name.lower().endswith(CDROM_FILE_SUFFIX):
        kwargs["device_type"] = "SCCD"
    elif file_name.lower().endswith(REMOVABLE_FILE_SUFFIX):
        kwargs["device_type"] = "SCRM"
    elif file_name.lower().endswith(HARDDRIVE_FILE_SUFFIX):
        kwargs["device_type"] = "SCHD"
 
    # Attempt to load the device config sidecar file:
    # same base path but .rascsi instead of the original suffix.
    from pathlib import Path
    file_name_base = str(Path(file_name).stem)
    device_config = Path(base_dir + file_name_base + ".rascsi")
    if device_config.is_file():
        process = read_sidecar(str(device_config))
        if process["status"] == False:
            flash(f"Failed to load the sidecar file {file_name_base}.rascsi", "error")
            flash(process["msg"], "error")
            return redirect(url_for("index"))
        conf = process["conf"]
        # CD-ROM drives have no inherent size
        if kwargs["device_type"] != "SCCD":
            conf_file_size = int(conf["blocks"]) * int(conf["block_size"])
            if conf_file_size != 0 and conf_file_size > int(file_size):
                flash(f"Failed to attach {file_name} to SCSI id {scsi_id}!", "error")
                flash(f"The file size {file_size} bytes needs to be at least {conf_file_size} bytes.", "error")
                return redirect(url_for("index"))
        kwargs["vendor"] = conf["vendor"]
        kwargs["product"] = conf["product"]
        kwargs["revision"] = conf["revision"]
        kwargs["block_size"] = conf["block_size"]

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

    devices = list_devices(scsi_id)

    # First check if any device at all was returned
    if devices["status"] == False:
        flash(f"No device attached to SCSI id {scsi_id}!", "error")
        return redirect(url_for("index"))
    # Looking at the first dict in list to get
    # the one and only device that should have been returned
    device = devices["device_list"][0]
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
    flash("Restarting the Pi momentarily...")
    reboot_pi()
    return redirect(url_for("index"))


@app.route("/rascsi/restart", methods=["POST"])
def rascsi_restart():
    server_info = get_server_info()
    rascsi_service("restart")
    flash("Restarting RaSCSI Service...")
    # Need to turn this into a list of strings from a list of ints
    reserve_scsi_ids([str(e) for e in server_info["reserved_ids"]])
    return redirect(url_for("index"))


@app.route("/pi/shutdown", methods=["POST"])
def shutdown():
    flash("Shutting down the Pi momentarily...")
    shutdown_pi()
    return redirect(url_for("index"))


@app.route("/files/download_to_iso", methods=["POST"])
def download_file():
    scsi_id = request.form.get("scsi_id")
    validate = validate_scsi_id(scsi_id)
    if validate["status"] == False:
        flash(validate["msg"], "error")
        return redirect(url_for("index"))

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
    size = (int(request.form.get("size")) * 1024 * 1024)
    file_type = request.form.get("type")

    process = create_new_image(file_name, file_type, size)
    if process["status"] == True:
        flash(f"Drive image created as {file_name}.{file_type}")
        flash(process["msg"])
        return redirect(url_for("index"))
    else:
        flash(f"Failed to create file {file_name}.{file_type}", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))


@app.route("/files/download", methods=["POST"])
def download():
    image = request.form.get("image")
    return send_file(base_dir + image, as_attachment=True)


@app.route("/files/delete", methods=["POST"])
def delete():
    file_name = request.form.get("image")

    process = delete_file(file_name)
    if process["status"] == True:
        flash(f"File {file_name} deleted!")
        flash(process["msg"])
    else:
        flash(f"Failed to delete file {file_name}!", "error")
        flash(process["msg"], "error")
        return redirect(url_for("index"))

    # Delete the sidecar file, if it exists
    from pathlib import Path
    file_name = str(Path(file_name).stem) + ".rascsi"
    sidecar = Path(base_dir + file_name)
    if sidecar.is_file():
        process = delete_file(file_name)
        if process["status"] == True:
            flash(f"File {file_name} deleted!")
            flash(process["msg"])
            return redirect(url_for("index"))
        else:
            flash(f"Failed to delete file {file_name}!", "error")
            flash(process["msg"], "error")
            return redirect(url_for("index"))

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
        # Reserve SCSI IDs on the backend side to prevent use
        # Expecting argv as a string of digits such as '017'
        reserve_scsi_ids(list(argv[1]))

    # Load the default configuration file, if found
    from pathlib import Path
    default_config_path = Path(base_dir + DEFAULT_CONFIG)
    if default_config_path.is_file():
        read_config(DEFAULT_CONFIG)

    import bjoern
    print("Serving rascsi-web...")
    bjoern.run(app, "0.0.0.0", 8080)

