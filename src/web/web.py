import io
import re
import sys

from flask import Flask, render_template, request, flash, url_for, redirect, send_file

from file_cmds import (
    create_new_image,
    download_file_to_iso,
    delete_file,
    unzip_file,
    download_image,
    write_config_csv,
    read_config_csv,
)
from pi_cmds import shutdown_pi, reboot_pi, running_version, rascsi_service
from ractl_cmds import (
    attach_image,
    list_devices,
    is_active,
    list_files,
    detach_by_id,
    eject_by_id,
    get_valid_scsi_ids,
    attach_daynaport,
    is_bridge_setup,
    daynaport_setup_bridge,
    list_config_files,
    detach_all,
    valid_file_suffix,
    valid_file_types
)
from settings import *

app = Flask(__name__)


@app.route("/")
def index():
    devices = list_devices()
    exclude_scsi_id = app.config.get('EXCLUDE_SCSI_ID')
    scsi_ids = get_valid_scsi_ids(devices, exclude_scsi_id)
    return render_template(
        "index.html",
        bridge_configured=is_bridge_setup("eth0"),
        devices=devices,
        active=is_active(),
        files=list_files(),
        config_files=list_config_files(),
        base_dir=base_dir,
        scsi_ids=scsi_ids,
        max_file_size=MAX_FILE_SIZE,
        exclude_scsi_id=str(exclude_scsi_id),
        version=running_version(),
    )


@app.route("/config/save", methods=["POST"])
def config_save():
    file_name = request.form.get("name") or "default"
    file_name = f"{base_dir}{file_name}.csv"

    write_config_csv(file_name)
    flash(f"Saved config to  {file_name}!")
    return redirect(url_for("index"))


@app.route("/config/load", methods=["POST"])
def config_load():
    file_name = request.form.get("name")
    file_name = f"{base_dir}{file_name}"

    if "load" in request.form:
        if read_config_csv(file_name):
            flash(f"Loaded config from  {file_name}!")
        else:
            flash(f"Failed to load  {file_name}!", "error")
    elif "delete" in request.form:
        if delete_file(file_name):
            flash(f"Deleted config  {file_name}!")
        else:
            flash(f"Failed to delete  {file_name}!", "error")

    return redirect(url_for("index"))


@app.route("/logs")
def logs():
    import subprocess

    lines = request.args.get("lines") or "100"
    process = subprocess.run(["journalctl", "-n", lines], capture_output=True)

    if process.returncode == 0:
        headers = {"content-type": "text/plain"}
        return process.stdout.decode("utf-8"), 200, headers
    else:
        flash("Failed to get logs")
        flash(process.stdout.decode("utf-8"), "stdout")
        flash(process.stderr.decode("utf-8"), "stderr")
        return redirect(url_for("index"))


@app.route("/daynaport/attach", methods=["POST"])
def daynaport_attach():
    scsi_id = request.form.get("scsi_id")
    process = attach_daynaport(scsi_id)
    if process.returncode == 0:
        flash(f"Attached DaynaPORT to SCSI id {scsi_id}!")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to attach DaynaPORT to SCSI id {scsi_id}!", "error")
        flash(process.stdout.decode("utf-8"), "stdout")
        flash(process.stderr.decode("utf-8"), "stderr")
        return redirect(url_for("index"))


@app.route("/daynaport/setup", methods=["POST"])
def daynaport_setup():
    # Future use for wifi
    interface = request.form.get("interface") or "eth0"
    process = daynaport_setup_bridge(interface)
    if process.returncode == 0:
        flash(f"Configured DaynaPORT bridge on {interface}!")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to configure DaynaPORT bridge on {interface}!", "error")
        flash(process.stdout.decode("utf-8"), "stdout")
        flash(process.stderr.decode("utf-8"), "stderr")
        return redirect(url_for("index"))


@app.route("/scsi/attach", methods=["POST"])
def attach():
    file_name = request.form.get("file_name")
    scsi_id = request.form.get("scsi_id")

    # Validate image type by suffix
    print("file_name", file_name)
    print("valid_file_types: ", valid_file_types)
    if re.match(valid_file_types, file_name):
        if file_name.lower().endswith((".iso", ".cdr", ".toast", ".img")):
            image_type = "SCCD"
        else:
            image_type = "SCHD"
    else:
        flash(f"Unknown file type. Valid files are: {', '.join(valid_file_suffix)}", "error")
        return redirect(url_for("index"))

    # Validate the SCSI ID
    if re.match("[0-7]", str(scsi_id)) == None:
        flash(f"Invalid SCSI ID. Should be a number between 0-7", "error")
        return redirect(url_for("index"))

    process = attach_image(scsi_id, file_name, image_type)
    if process.returncode == 0:
        flash(f"Attached {file_name} to SCSI id {scsi_id}!")
        return redirect(url_for("index"))
    else:
        flash(f"Failed to attach {file_name} to SCSI id {scsi_id}!", "error")
        flash(process.stdout.decode("utf-8"), "stdout")
        flash(process.stderr.decode("utf-8"), "stderr")
        return redirect(url_for("index"))


@app.route("/scsi/detach_all", methods=["POST"])
def detach_all_devices():
    detach_all()
    flash("Detached all SCSI devices!")
    return redirect(url_for("index"))


@app.route("/scsi/detach", methods=["POST"])
def detach():
    scsi_id = request.form.get("scsi_id")
    process = detach_by_id(scsi_id)
    if process.returncode == 0:
        flash("Detached SCSI id " + scsi_id + "!")
        return redirect(url_for("index"))
    else:
        flash("Failed to detach SCSI id " + scsi_id + "!", "error")
        flash(process.stdout, "stdout")
        flash(process.stderr, "stderr")
        return redirect(url_for("index"))


@app.route("/scsi/eject", methods=["POST"])
def eject():
    scsi_id = request.form.get("scsi_id")
    process = eject_by_id(scsi_id)
    if process.returncode == 0:
        flash("Ejected scsi id " + scsi_id + "!")
        return redirect(url_for("index"))
    else:
        flash("Failed to eject SCSI id " + scsi_id + "!", "error")
        flash(process.stdout, "stdout")
        flash(process.stderr, "stderr")
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
    if process.returncode == 0:
        flash("File Downloaded")
        return redirect(url_for("index"))
    else:
        flash("Failed to download file", "error")
        flash(process.stdout, "stdout")
        flash(process.stderr, "stderr")
        return redirect(url_for("index"))


@app.route("/files/download_image", methods=["POST"])
def download_img():
    url = request.form.get("url")
    # TODO: error handling
    download_image(url)
    flash("File Downloaded")
    return redirect(url_for("index"))


@app.route("/files/upload/<filename>", methods=["POST"])
def upload_file(filename):
    if not filename:
        flash("No file provided.", "error")
        return redirect(url_for("index"))

    file_path = os.path.join(app.config["UPLOAD_FOLDER"], filename)
    if os.path.isfile(file_path):
        flash(f"{filename} already exists.", "error")
        return redirect(url_for("index"))

    binary_new_file = "bx"
    with open(file_path, binary_new_file, buffering=io.DEFAULT_BUFFER_SIZE) as f:
        chunk_size = io.DEFAULT_BUFFER_SIZE
        while True:
            chunk = request.stream.read(chunk_size)
            if len(chunk) == 0:
                break
            f.write(chunk)
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
    os.makedirs(app.config["UPLOAD_FOLDER"], exist_ok=True)
    app.config["MAX_CONTENT_LENGTH"] = MAX_FILE_SIZE
    if len(sys.argv) >= 2:
        app.config["EXCLUDE_SCSI_ID"] = int(sys.argv[1])
    else:
        app.config["EXCLUDE_SCSI_ID"] = None
    
    read_config_csv(f"{base_dir}default.csv")

    import bjoern
    print("Serving rascsi-web...")
    bjoern.run(app, "0.0.0.0", 8080)

