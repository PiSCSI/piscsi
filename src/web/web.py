from flask import Flask, render_template, request, flash, url_for, redirect, send_file
from werkzeug.utils import secure_filename

from file_cmds import (
    create_new_image,
    download_file_to_iso,
    delete_image,
    unzip_file,
    download_image, make_iso_from_file,
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
    detach_all, load_config,
)
from settings import *

app = Flask(__name__)


def build_redirect():
    base_path = request.args.get("dir") or ""
    return redirect(url_for('index', dir=base_path))


@app.route("/")
def index():
    sub_dir = request.args.get("dir") or ""
    devices = list_devices()
    scsi_ids = get_valid_scsi_ids(devices)
    return render_template(
        "index.html",
        bridge_configured=is_bridge_setup("eth0"),
        devices=devices,
        active=is_active(),
        files=list_files(sub_dir),
        config_files=list_config_files(),
        base_dir=base_dir,
        scsi_ids=scsi_ids,
        max_file_size=MAX_FILE_SIZE,
        version=running_version(),
        sub_dir=sub_dir,
    )


@app.route("/config/save", methods=["POST"])
def config_save():
    file_name = request.form.get("name") or "default"
    file_name = f"{base_dir}{file_name}.config"
    import csv

    with open(file_name, "w") as csv_file:
        writer = csv.writer(csv_file)
        for device in list_devices():
            if device["type"] != "-":
                writer.writerow(device.values())
    flash(f"Saved config to  {file_name}!")
    return build_redirect()


@app.route("/config/load", methods=["POST"])
def config_load():
    file_name = request.form.get("name") or "default.config"
    file_name = f"{base_dir}{file_name}"
    detach_all()
    load_config(file_name)
    flash(f"Loaded config from  {file_name}!")
    return build_redirect()


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
        return build_redirect()


@app.route("/daynaport/attach", methods=["POST"])
def daynaport_attach():
    scsi_id = request.form.get("scsi_id")
    process = attach_daynaport(scsi_id)
    if process.returncode == 0:
        flash(f"Attached DaynaPORT to SCSI id {scsi_id}!")
        return build_redirect()
    else:
        flash(f"Failed to attach DaynaPORT to SCSI id {scsi_id}!", "error")
        flash(process.stdout.decode("utf-8"), "stdout")
        flash(process.stderr.decode("utf-8"), "stderr")
        return build_redirect()


@app.route("/daynaport/setup", methods=["POST"])
def daynaport_setup():
    # Future use for wifi
    interface = request.form.get("interface") or "eth0"
    process = daynaport_setup_bridge(interface)
    if process.returncode == 0:
        flash(f"Configured DaynaPORT bridge on {interface}!")
        return build_redirect()
    else:
        flash(f"Failed to configure DaynaPORT bridge on {interface}!", "error")
        flash(process.stdout.decode("utf-8"), "stdout")
        flash(process.stderr.decode("utf-8"), "stderr")
        return build_redirect()


@app.route("/scsi/attach", methods=["POST"])
def attach():
    file_name = request.form.get("file_name")
    scsi_id = request.form.get("scsi_id")

    # Validate image type by suffix
    if file_name.lower().endswith(".iso") or file_name.lower().endswith("iso"):
        image_type = "cd"
    elif file_name.lower().endswith(".hda"):
        image_type = "hd"
    else:
        flash("Unknown file type. Valid files are .iso, .hda, .cdr", "error")
        return build_redirect()

    process = attach_image(scsi_id, file_name, image_type)
    if process.returncode == 0:
        flash(f"Attached {file_name} to SCSI id {scsi_id}!")
        return build_redirect()
    else:
        flash(f"Failed to attach {file_name} to SCSI id {scsi_id}!", "error")
        flash(process.stdout.decode("utf-8"), "stdout")
        flash(process.stderr.decode("utf-8"), "stderr")
        return build_redirect()


@app.route("/scsi/detach_all", methods=["POST"])
def detach_all_devices():
    detach_all()
    flash("Detached all SCSI devices!")
    return build_redirect()


@app.route("/scsi/detach", methods=["POST"])
def detach():
    scsi_id = request.form.get("scsi_id")
    process = detach_by_id(scsi_id)
    if process.returncode == 0:
        flash("Detached SCSI id " + scsi_id + "!")
        return build_redirect()
    else:
        flash("Failed to detach SCSI id " + scsi_id + "!", "error")
        flash(process.stdout, "stdout")
        flash(process.stderr, "stderr")
        return build_redirect()


@app.route("/scsi/eject", methods=["POST"])
def eject():
    scsi_id = request.form.get("scsi_id")
    process = eject_by_id(scsi_id)
    if process.returncode == 0:
        flash("Ejected scsi id " + scsi_id + "!")
        return build_redirect()
    else:
        flash("Failed to eject SCSI id " + scsi_id + "!", "error")
        flash(process.stdout, "stdout")
        flash(process.stderr, "stderr")
        return build_redirect()


@app.route("/pi/reboot", methods=["POST"])
def restart():
    reboot_pi()
    flash("Restarting...")
    return build_redirect()


@app.route("/rascsi/restart", methods=["POST"])
def rascsi_restart():
    rascsi_service("restart")
    flash("Restarting RaSCSI Service...")
    return build_redirect()


@app.route("/pi/shutdown", methods=["POST"])
def shutdown():
    shutdown_pi()
    flash("Shutting down...")
    return build_redirect()


@app.route("/files/download_to_iso", methods=["POST"])
def download_file():
    scsi_id = request.form.get("scsi_id")
    url = request.form.get("url")
    process = download_file_to_iso(scsi_id, url)
    if process.returncode == 0:
        flash("File Downloaded")
        return build_redirect()
    else:
        flash("Failed to download file", "error")
        flash(process.stdout, "stdout")
        flash(process.stderr, "stderr")
        return build_redirect()


@app.route("/files/download_image", methods=["POST"])
def download_img():
    url = request.form.get("url")
    # TODO: error handling
    download_image(url)
    flash("File Downloaded")
    return build_redirect()


@app.route("/files/upload", methods=["POST"])
def upload_file():
    if "file" not in request.files:
        flash("No file part", "error")
        return redirect(url_for("index"))
    file = request.files["file"]
    iso = request.form.get("iso")
    if file:
        base_path = request.args.get("dir") or ""
        file_name = secure_filename(file.filename)
        full_path = os.path.join(app.config["UPLOAD_FOLDER"], base_path, file_name)
        file.save(full_path)
        if iso == "on":
            iso_proc = make_iso_from_file(full_path)
            if iso_proc.returncode != 0:
                flash(f"Failed to create ISO from file {full_path}", "error")
                return redirect(url_for("index"))
        flash("File Uploaded")
        return build_redirect()
    else:
        flash("No file selected", "error")
        return build_redirect()


@app.route("/files/create", methods=["POST"])
def create_file():
    file_name = request.form.get("file_name")
    size = request.form.get("size")
    type = request.form.get("type")
    sub_dir = request.args.get("dir") or ""

    process = create_new_image(file_name, type, size, sub_dir)
    if process.returncode == 0:
        flash("Drive created")
        return build_redirect()
    else:
        flash("Failed to create file", "error")
        flash(process.stdout, "stdout")
        flash(process.stderr, "stderr")
        return build_redirect()


@app.route("/files/download", methods=["POST"])
def download():
    image = request.form.get("image")
    return send_file(base_dir + image, as_attachment=True)


@app.route("/files/delete", methods=["POST"])
def delete():
    image = request.form.get("image")
    if delete_image(image):
        flash("File " + image + " deleted")
        return build_redirect()
    else:
        flash("Failed to Delete " + image, "error")
        return build_redirect()


@app.route("/files/unzip", methods=["POST"])
def unzip():
    image = request.form.get("image")

    if unzip_file(image):
        flash("Unzipped file " + image)
        return build_redirect()
    else:
        flash("Failed to unzip " + image, "error")
        return build_redirect()


if __name__ == "__main__":
    app.secret_key = "rascsi_is_awesome_insecure_secret_key"
    app.config["SESSION_TYPE"] = "filesystem"
    app.config["UPLOAD_FOLDER"] = base_dir
    os.makedirs(app.config["UPLOAD_FOLDER"], exist_ok=True)
    app.config["MAX_CONTENT_LENGTH"] = MAX_FILE_SIZE

    default_config = f"{base_dir}default.config"
    if os.path.exists(default_config):
        print(f"Loading default config: {default_config}")
        load_config(default_config, True)

    from waitress import serve

    serve(app, host="0.0.0.0", port=8080)
