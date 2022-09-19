import pytest
import uuid
import os

CFG_DIR = "/home/pi/.config/rascsi"
IMAGES_DIR = "/home/pi/images"
AFP_DIR = "/home/pi/afpshare"
SCSI_ID = 6
FILE_SIZE_1_MIB = 1048576
STATUS_SUCCESS = "success"
STATUS_ERROR = "error"


@pytest.fixture(scope="function")
def create_test_image(request, http_client):
    images = []

    def create(image_type="hds", size=1, auto_delete=True):
        file_prefix = str(uuid.uuid4())
        file_name = f"{file_prefix}.{image_type}"

        response = http_client.post(
            "/files/create",
            data={
                "file_name": file_prefix,
                "type": image_type,
                "size": size,
            },
        )

        if response.json()["status"] != STATUS_SUCCESS:
            raise Exception("Failed to create temporary image")

        if auto_delete:
            images.append(file_name)

        return file_name

    def delete():
        for image in images:
            http_client.post("/files/delete", data={"file_name": image})

    request.addfinalizer(delete)
    return create


@pytest.fixture(scope="function")
def list_files(http_client):
    def files():
        return [f["name"] for f in http_client.get("/").json()["data"]["files"]]

    return files


@pytest.fixture(scope="function")
def list_attached_images(http_client):
    def files():
        return http_client.get("/").json()["data"]["attached_images"]

    return files


@pytest.fixture(scope="function")
def delete_file(http_client):
    def delete(file_name):
        http_client.post("/files/delete", data={"file_name": file_name})

    return delete


@pytest.fixture(scope="function")
def detach_devices(http_client):
    def detach():
        response = http_client.post("/scsi/detach_all")
        if response.json()["status"] == STATUS_SUCCESS:
            return True
        raise Exception("Failed to detach SCSI devices")

    return detach


"""
AUTHENTICATION
"""


# route("/login", methods=["POST"])
def test_login_with_valid_credentials(pytestconfig, http_client_unauthenticated):
    response = http_client_unauthenticated.post(
        "/login",
        data={
            "username": pytestconfig.getoption("rascsi_username"),
            "password": pytestconfig.getoption("rascsi_password"),
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS


# route("/login", methods=["POST"])
def test_login_with_invalid_credentials(http_client_unauthenticated):
    response = http_client_unauthenticated.post(
        "/login",
        data={
            "username": "__INVALID_USER__",
            "password": "__INVALID_PASS__",
        },
    )

    response_data = response.json()

    assert response.status_code == 401
    assert response_data["status"] == STATUS_ERROR
    assert response_data["messages"][0]["message"] == (
        "You must log in with valid credentials for a user in the 'rascsi' group"
    )


# route("/logout")
def test_logout(http_client):
    response = http_client.get("/logout")
    response.status_code == 200


"""
DEVICE OPERATIONS
"""


# route("/scsi/attach", methods=["POST"])
def test_attach_image(http_client, create_test_image, detach_devices):
    test_image = create_test_image()

    response = http_client.post(
        "/scsi/attach",
        data={
            "file_name": test_image,
            "file_size": FILE_SIZE_1_MIB,
            "scsi_id": SCSI_ID,
            "unit": 0,
            "type": "SCHD",
        },
    )
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == (
        f"Attached {test_image} as Hard Disk to SCSI ID {SCSI_ID} LUN 0"
    )

    # Cleanup
    detach_devices()


# route("/scsi/attach_device", methods=["POST"])
@pytest.mark.parametrize(
    "device_name,device_config",
    [
        # TODO: Fix networking in container, else SCBR attachment fails
        # ("X68000 Host Bridge", {"type": "SCBR", "interface": "eth0", "inet": "10.10.20.1/24"}),
        ("DaynaPORT SCSI/Link", {"type": "SCDP", "interface": "eth0", "inet": "10.10.20.1/24"}),
        ("Host Services", {"type": "SCHS"}),
        ("Printer", {"type": "SCLP", "timeout": 30, "cmd": "lp -oraw %f"}),
    ],
)
def test_attach_device(http_client, detach_devices, device_name, device_config):
    device_config["scsi_id"] = SCSI_ID
    device_config["unit"] = 0

    response = http_client.post(
        "/scsi/attach_device",
        data=device_config,
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == (
        f"Attached {device_name} to SCSI ID {SCSI_ID} LUN 0"
    )

    # Cleanup
    detach_devices()


# route("/scsi/detach", methods=["POST"])
def test_detach_device(http_client, create_test_image):
    test_image = create_test_image()

    http_client.post(
        "/scsi/attach",
        data={
            "file_name": test_image,
            "file_size": FILE_SIZE_1_MIB,
            "scsi_id": SCSI_ID,
            "unit": 0,
            "type": "SCHD",
        },
    )

    response = http_client.post(
        "/scsi/detach",
        data={
            "scsi_id": SCSI_ID,
            "unit": 0,
        },
    )

    response_data = response.json()

    response.status_code == 200
    response_data["status"] == STATUS_SUCCESS
    response_data["messages"][0]["message"] == f"Detached SCSI ID {SCSI_ID} LUN 0"


# route("/scsi/detach_all", methods=["POST"])
def test_detach_all_devices(http_client, create_test_image, list_attached_images):
    test_images = []
    scsi_ids = [4, 5, 6]

    for scsi_id in scsi_ids:
        test_image = create_test_image()
        test_images.append(test_image)

        http_client.post(
            "/scsi/attach",
            data={
                "file_name": test_image,
                "file_size": FILE_SIZE_1_MIB,
                "scsi_id": scsi_id,
                "unit": 0,
                "type": "SCHD",
            },
        )

    assert list_attached_images() == test_images

    response = http_client.post("/scsi/detach_all")
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert list_attached_images() == []


# route("/scsi/eject", methods=["POST"])
def test_eject_device(http_client, create_test_image, detach_devices):
    test_image = create_test_image()

    http_client.post(
        "/scsi/attach",
        data={
            "file_name": test_image,
            "file_size": FILE_SIZE_1_MIB,
            "scsi_id": SCSI_ID,
            "unit": 0,
            "type": "SCCD",  # CD-ROM
        },
    )

    response = http_client.post(
        "/scsi/eject",
        data={
            "scsi_id": SCSI_ID,
            "unit": 0,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Ejected SCSI ID {SCSI_ID} LUN 0"

    # Cleanup
    detach_devices()


# route("/scsi/info", methods=["POST"])
def test_show_device_info(http_client, create_test_image, detach_devices):
    test_image = create_test_image()

    http_client.post(
        "/scsi/attach",
        data={
            "file_name": test_image,
            "file_size": FILE_SIZE_1_MIB,
            "scsi_id": SCSI_ID,
            "unit": 0,
            "type": "SCHD",
        },
    )

    response = http_client.post(
        "/scsi/info",
        data={
            "scsi_id": SCSI_ID,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "device_info" in response_data["data"]
    assert response_data["data"]["device_info"]["file"] == f"{IMAGES_DIR}/{test_image}"

    # Cleanup
    detach_devices()


# route("/scsi/reserve", methods=["POST"])
# route("/scsi/release", methods=["POST"])
def test_reserve_and_release_device(http_client):
    scsi_id = 0

    response = http_client.post(
        "/scsi/reserve",
        data={
            "scsi_id": scsi_id,
            "memo": "TEST",
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Reserved SCSI ID {scsi_id}"

    response = http_client.post(
        "/scsi/release",
        data={
            "scsi_id": scsi_id,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == (
        f"Released the reservation for SCSI ID {scsi_id}"
    )


"""
FILE OPERATIONS
"""


# route("/files/create", methods=["POST"])
def test_create_file(http_client, list_files, delete_file):
    file_prefix = str(uuid.uuid4())
    file_name = f"{file_prefix}.hds"

    response = http_client.post(
        "/files/create",
        data={
            "file_name": file_prefix,
            "type": "hds",
            "size": 1,
        },
    )

    response_data = response.json()

    assert response.status_code == 201
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["data"]["image"] == file_name
    assert response_data["messages"][0]["message"] == f"Image file created: {file_name}"
    assert file_name in list_files()

    # Cleanup
    delete_file(file_name)


# route("/files/rename", methods=["POST"])
def test_rename_file(http_client, create_test_image, list_files, delete_file):
    original_file = create_test_image(auto_delete=False)
    renamed_file = f"{uuid.uuid4()}.rename"

    response = http_client.post(
        "/files/rename",
        data={"file_name": original_file, "new_file_name": renamed_file},
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Image file renamed to: {renamed_file}"
    assert renamed_file in list_files()

    # Cleanup
    delete_file(renamed_file)


# route("/files/copy", methods=["POST"])
def test_copy_file(http_client, create_test_image, list_files, delete_file):
    original_file = create_test_image()
    copy_file = f"{uuid.uuid4()}.copy"

    response = http_client.post(
        "/files/copy",
        data={
            "file_name": original_file,
            "copy_file_name": copy_file,
        },
    )

    response_data = response.json()
    files = list_files()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Copy of image file saved as: {copy_file}"
    assert original_file in files
    assert copy_file in files

    # Cleanup
    delete_file(copy_file)


# route("/files/delete", methods=["POST"])
def test_delete_file(http_client, create_test_image, list_files):
    file_name = create_test_image()

    response = http_client.post("/files/delete", data={"file_name": file_name})

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Image file deleted: {file_name}"
    assert file_name not in list_files()


# route("/files/extract_image", methods=["POST"])
# TODO: Add test files for all supported formats
def test_extract_file(httpserver, http_client, list_files, delete_file):
    image_file_name = "test_image.hds"
    zip_file_name = "test_image.hds.zip"
    http_path = f"/images/{zip_file_name}"
    url = httpserver.url_for(http_path)

    with open(f"tests/assets/{zip_file_name}", mode="rb") as file:
        zip_file_data = file.read()

    httpserver.expect_request(http_path).respond_with_data(
        zip_file_data,
        mimetype="application/octet-stream",
    )

    http_client.post(
        "/files/download_to_images",
        data={
            "url": url,
        },
    )

    response = http_client.post(
        "/files/extract_image",
        data={
            "archive_file": zip_file_name,
            "archive_members": image_file_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == "Extracted 1 file(s)"
    assert image_file_name in list_files()

    # Cleanup
    delete_file(zip_file_name)
    delete_file(image_file_name)


# route("/files/upload", methods=["POST"])
def test_upload_file(http_client, delete_file):
    file_name = f"{uuid.uuid4()}.test"

    with open("tests/assets/test_image.hds", mode="rb") as file:
        file.seek(0, os.SEEK_END)
        file_size = file.tell()
        file.seek(0, 0)

        number_of_chunks = 4

        # Note: The test file needs to be cleanly divisible by the chunk size
        chunk_size = int(file_size / number_of_chunks)

        for chunk_number in range(0, 4):
            if chunk_number == 0:
                chunk_byte_offset = 0
            else:
                chunk_byte_offset = chunk_number * chunk_size

            form_data = {
                "dzuuid": str(uuid.uuid4()),
                "dzchunkindex": chunk_number,
                "dzchunksize": chunk_size,
                "dzchunkbyteoffset": chunk_byte_offset,
                "dztotalfilesize": file_size,
                "dztotalchunkcount": number_of_chunks,
            }

            file_data = {"file": (file_name, file.read(chunk_size))}

            response = http_client.post(
                "/files/upload",
                data=form_data,
                files=file_data,
            )

            assert response.status_code == 200
            assert response.text == "File upload successful!"

    file = [f for f in http_client.get("/").json()["data"]["files"] if f["name"] == file_name][0]

    assert file["size"] == file_size

    # Cleanup
    delete_file(file_name)


# route("/files/download", methods=["POST"])
def test_download_file(http_client, create_test_image):
    file_name = create_test_image()

    response = http_client.post("/files/download", data={"file": f"{IMAGES_DIR}/{file_name}"})

    assert response.status_code == 200
    assert response.headers["content-type"] == "application/octet-stream"
    assert response.headers["content-disposition"] == f"attachment; filename={file_name}"
    assert response.headers["content-length"] == str(FILE_SIZE_1_MIB)


# route("/files/download_to_afp", methods=["POST"])
def test_download_url_to_afp_dir(httpserver, http_client):
    file_name = str(uuid.uuid4())
    http_path = f"/images/{file_name}"
    url = httpserver.url_for(http_path)

    with open("tests/assets/test_image.hds", mode="rb") as file:
        file_data = file.read()

    httpserver.expect_request(http_path).respond_with_data(
        file_data,
        mimetype="application/octet-stream",
    )

    response = http_client.post(
        "/files/download_to_afp",
        data={
            "url": url,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"{file_name} downloaded to {AFP_DIR}"


# route("/files/download_to_images", methods=["POST"])
def test_download_url_to_images_dir(httpserver, http_client, list_files, delete_file):
    file_name = str(uuid.uuid4())
    http_path = f"/images/{file_name}"
    url = httpserver.url_for(http_path)

    with open("tests/assets/test_image.hds", mode="rb") as file:
        test_file_data = file.read()

    httpserver.expect_request(http_path).respond_with_data(
        test_file_data,
        mimetype="application/octet-stream",
    )

    response = http_client.post(
        "/files/download_to_images",
        data={
            "url": url,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert file_name in list_files()
    assert response_data["messages"][0]["message"] == f"{file_name} downloaded to {IMAGES_DIR}"

    # Cleanup
    delete_file(file_name)


# route("/files/download_to_iso", methods=["POST"])
def test_download_url_to_iso(
    httpserver,
    http_client,
    list_files,
    list_attached_images,
    detach_devices,
    delete_file,
):
    test_file_name = str(uuid.uuid4())
    iso_file_name = f"{test_file_name}.iso"

    http_path = f"/images/{test_file_name}"
    url = httpserver.url_for(http_path)

    with open("tests/assets/test_image.hds", mode="rb") as file:
        test_file_data = file.read()

    httpserver.expect_request(http_path).respond_with_data(
        test_file_data,
        mimetype="application/octet-stream",
    )

    response = http_client.post(
        "/files/download_to_iso",
        data={
            "scsi_id": SCSI_ID,
            "type": "-hfs",
            "url": url,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert iso_file_name in list_files()
    assert iso_file_name in list_attached_images()

    m = response_data["messages"]
    assert m[0]["message"] == 'Created CD-ROM ISO image with arguments "-hfs"'
    assert m[1]["message"] == f"Saved image as: {IMAGES_DIR}/{iso_file_name}"
    assert m[2]["message"] == f"Attached to SCSI ID {SCSI_ID}"

    # Cleanup
    detach_devices()
    delete_file(iso_file_name)


"""
NAMED DEVICES
"""


# route("/drive/list", methods=["GET"])
def test_show_named_drive_presets(http_client):
    response = http_client.get("/drive/list")
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "cd_conf" in response_data["data"]
    assert "hd_conf" in response_data["data"]
    assert "rm_conf" in response_data["data"]


# route("/drive/cdrom", methods=["POST"])
def test_create_cdrom_properties_file(http_client):
    file_name = f"{uuid.uuid4()}.iso"

    response = http_client.post(
        "/drive/cdrom",
        data={
            "vendor": "TEST_AAA",
            "product": "TEST_BBB",
            "revision": "1.0A",
            "block_size": 2048,
            "file_name": file_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == (
        f"File created: {CFG_DIR}/{file_name}.properties"
    )


# route("/drive/create", methods=["POST"])
def test_create_image_with_properties_file(http_client, delete_file):
    file_prefix = str(uuid.uuid4())
    file_name = f"{file_prefix}.hds"

    response = http_client.post(
        "/drive/create",
        data={
            "vendor": "TEST_AAA",
            "product": "TEST_BBB",
            "revision": "1.0A",
            "block_size": 512,
            "size": FILE_SIZE_1_MIB,
            "file_type": "hds",
            "file_name": file_prefix,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Image file created: {file_name}"

    # Cleanup
    delete_file(file_name)


"""
INDEX & STATIC
"""


# route("/")
def test_index(http_client):
    response = http_client.get("/")
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "devices" in response_data["data"]


# route("/pwa/<path:pwa_path>")
def test_pwa_route(http_client):
    response = http_client.get("/pwa/favicon.ico")

    assert response.status_code == 200
    assert response.headers["content-disposition"] == "inline; filename=favicon.ico"


"""
SETTINGS
"""


# route("/language", methods=["POST"])
@pytest.mark.parametrize(
    "locale,confirm_message",
    [
        ("de", "Webinterface-Sprache auf Deutsch geändert"),
        ("es", "Se ha cambiado el lenguaje de la Interfaz Web a español"),
        ("fr", "Langue de l’interface web changée pour français"),
        ("sv", "Bytte webbgränssnittets språk till svenska"),
        ("en", "Changed Web Interface language to English"),
    ],
)
def test_set_language(http_client, locale, confirm_message):
    response = http_client.post(
        "/language",
        data={
            "locale": locale,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == confirm_message


# route("/logs/level", methods=["POST"])
@pytest.mark.parametrize("level", ["trace", "debug", "info", "warn", "err", "critical", "off"])
def test_set_log_level(http_client, level):
    response = http_client.post(
        "/logs/level",
        data={
            "level": level,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Log level set to {level}"

    # Cleanup
    http_client.post(
        "/logs/level",
        data={
            "level": "debug",
        },
    )


# route("/logs/show", methods=["POST"])
def test_show_logs(http_client):
    response = http_client.post(
        "/logs/show",
        data={
            "lines": 100,
            "scope": "",
        },
    )

    assert response.status_code == 200
    assert response.text == "-- No entries --\n"


# route("/config/load", methods=["POST"])
def test_save_load_and_delete_configs(http_client):
    config_name = str(uuid.uuid4())
    config_json_file = f"{config_name}.json"
    reserved_scsi_id = 0
    reservation_memo = str(uuid.uuid4())

    # Confirm the initial state
    assert http_client.get("/").json()["data"]["RESERVATIONS"][0] == ""

    # Save the initial state to a config
    response = http_client.post(
        "/config/save",
        data={
            "name": config_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == (
        f"File created: {CFG_DIR}/{config_json_file}"
    )

    assert config_json_file in http_client.get("/").json()["data"]["config_files"]

    # Modify the state
    http_client.post(
        "/scsi/reserve",
        data={
            "scsi_id": reserved_scsi_id,
            "memo": reservation_memo,
        },
    )

    assert http_client.get("/").json()["data"]["RESERVATIONS"][0] == reservation_memo

    # Load the saved config
    response = http_client.post(
        "/config/load",
        data={
            "name": config_json_file,
            "load": True,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == (
        f"Loaded configurations from: {CFG_DIR}/{config_json_file}"
    )

    # Confirm the application has returned to its initial state
    assert http_client.get("/").json()["data"]["RESERVATIONS"][0] == ""

    # Delete the saved config
    response = http_client.post(
        "/config/load",
        data={
            "name": config_json_file,
            "delete": True,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == (
        f"File deleted: {CFG_DIR}/{config_json_file}"
    )

    assert config_json_file not in http_client.get("/").json()["data"]["config_files"]
