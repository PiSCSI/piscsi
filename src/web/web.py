import os

from flask import Flask, render_template, request, flash, url_for, redirect, send_file
from werkzeug.utils import secure_filename

from file_cmds import create_new_image, download_file_to_iso, delete_image, unzip_file, download_image
from pi_cmds import shutdown_pi, reboot_pi, running_version, rascsi_service
from ractl_cmds import attach_image, list_devices, is_active, list_files, detach_by_id, eject_by_id, get_valid_scsi_ids

app = Flask(__name__)
MAX_FILE_SIZE = 1024 * 1024 * 1024 * 2 # 2gb
base_dir = "/home/pi/images/"  # Default


@app.route('/')
def index():
    devices = list_devices()
    scsi_ids = get_valid_scsi_ids(devices)
    return render_template('index.html',
                           devices=devices,
                           active=is_active(),
                           files=list_files(),
                           base_dir=base_dir,
                           scsi_ids=scsi_ids,
                           max_file_size=MAX_FILE_SIZE,
                           version=running_version())


def get_valid_scsi_ids(devices):
    invalid_list = EXCLUDE_SCSI_IDS.copy()
    for device in devices:
        if device['file'] != "NO MEDIA" and device['file'] != "-":
            invalid_list.append(int(device['id']))

    valid_list = list(range(8))
    for id in invalid_list:
        valid_list.remove(id)
    valid_list.reverse()

    return valid_list


@app.route('/scsi/attach', methods=['POST'])
def attach():
    file_name = request.form.get('file_name')
    scsi_id = request.form.get('scsi_id')

    # Validate image type by suffix
    if file_name.lower().endswith('.iso') or file_name.lower().endswith('iso'):
        image_type = "cd"
    elif file_name.lower().endswith('.hda'):
        image_type = "hd"
    else:
        flash(u'Unknown file type. Valid files are .iso, .hda, .cdr', 'error')
        return redirect(url_for('index'))

    process = attach_image(scsi_id, file_name, image_type)
    if process.returncode == 0:
        flash('Attached '+ file_name + " to scsi id " + scsi_id + "!")
        return redirect(url_for('index'))
    else:
        flash(u'Failed to attach '+ file_name + " to scsi id " + scsi_id + "!", 'error')
        print(process.stdout.decode("utf-8"))
        print(process.stderr.decode("utf-8"))
        flash(process.stdout.decode("utf-8"), 'stdout')
        flash(process.stderr.decode("utf-8"), 'stderr')
        return redirect(url_for('index'))


@app.route('/scsi/detach', methods=['POST'])
def detach():
    scsi_id = request.form.get('scsi_id')
    process = detach_by_id(scsi_id)
    if process.returncode == 0:
        flash("Detached scsi id " + scsi_id + "!")
        return redirect(url_for('index'))
    else:
        flash(u"Failed to detach scsi id " + scsi_id + "!", 'error')
        flash(process.stdout, 'stdout')
        flash(process.stderr, 'stderr')
        return redirect(url_for('index'))


@app.route('/scsi/eject', methods=['POST'])
def eject():
    scsi_id = request.form.get('scsi_id')
    process = eject_by_id(scsi_id)
    if process.returncode == 0:
        flash("Ejected scsi id " + scsi_id + "!")
        return redirect(url_for('index'))
    else:
        flash(u"Failed to eject scsi id " + scsi_id + "!", 'error')
        flash(process.stdout, 'stdout')
        flash(process.stderr, 'stderr')
        return redirect(url_for('index'))


@app.route('/pi/reboot', methods=['POST'])
def restart():
    reboot_pi()
    flash("Restarting...")
    return redirect(url_for('index'))


@app.route('/rascsi/restart', methods=['POST'])
def rascsi_restart():
    rascsi_service("restart")
    flash("Restarting RaSCSI Service...")
    return redirect(url_for('index'))


@app.route('/pi/shutdown', methods=['POST'])
def shutdown():
    shutdown_pi()
    flash("Shutting down...")
    return redirect(url_for('index'))


@app.route('/files/download_to_iso', methods=['POST'])
def download_file():
    scsi_id = request.form.get('scsi_id')
    url = request.form.get('url')
    process = download_file_to_iso(scsi_id, url)
    if process.returncode == 0:
        flash("File Downloaded")
        return redirect(url_for('index'))
    else:
        flash(u"Failed to download file", 'error')
        flash(process.stdout, 'stdout')
        flash(process.stderr, 'stderr')
        return redirect(url_for('index'))


@app.route('/files/download_image', methods=['POST'])
def download_img():
    url = request.form.get('url')
    # TODO: error handling
    download_image(url)
    flash("File Downloaded")
    return redirect(url_for('index'))


@app.route('/files/upload', methods=['POST'])
def upload_file():
    if 'file' not in request.files:
        flash('No file part', 'error')
        return redirect(url_for('index'))
    file = request.files['file']
    if file:
        filename = secure_filename(file.filename)
        file.save(os.path.join(app.config['UPLOAD_FOLDER'], filename))
        return redirect(url_for('index', filename=filename))


@app.route('/files/create', methods=['POST'])
def create_file():
    file_name = request.form.get('file_name')
    size = request.form.get('size')
    type = request.form.get('type')

    process = create_new_image(file_name, type, size)
    if process.returncode == 0:
        flash("Drive created")
        return redirect(url_for('index'))
    else:
        flash(u"Failed to create file", 'error')
        flash(process.stdout, 'stdout')
        flash(process.stderr, 'stderr')
        return redirect(url_for('index'))


@app.route('/files/download', methods=['POST'])
def download():
    image = request.form.get('image')
    return send_file(base_dir + image, as_attachment=True)


@app.route('/files/delete', methods=['POST'])
def delete():
    image = request.form.get('image')
    if delete_image(image):
        flash("File " + image + " deleted")
        return redirect(url_for('index'))
    else:
        flash(u"Failed to Delete " + image, 'error')
        return redirect(url_for('index'))


@app.route('/files/unzip', methods=['POST'])
def unzip():
    image = request.form.get('image')

    if unzip_file(image):
        flash("Unzipped file " + image)
        return redirect(url_for('index'))
    else:
        flash(u"Failed to unzip " + image, 'error')
        return redirect(url_for('index'))


if __name__ == "__main__":
    app.secret_key = 'rascsi_is_awesome_insecure_secret_key'
    app.config['SESSION_TYPE'] = 'filesystem'
    app.config['UPLOAD_FOLDER'] = base_dir
    os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)
    app.config['MAX_CONTENT_LENGTH'] = MAX_FILE_SIZE

    from waitress import serve
    serve(app, host="0.0.0.0", port=8080)