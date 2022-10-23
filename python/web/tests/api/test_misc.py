import uuid

from conftest import (
    FILE_SIZE_1_MIB,
    STATUS_SUCCESS,
)


# route("/")
def test_index(http_client):
    response = http_client.get("/")
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "devices" in response_data["data"]


# route("/env")
def test_get_env_info(http_client):
    response = http_client.get("/env")
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "running_env" in response_data["data"]


# route("/pwa/<path:pwa_path>")
def test_pwa_route(http_client):
    response = http_client.get("/pwa/favicon.ico")

    assert response.status_code == 200
    assert response.headers["content-disposition"] == "inline; filename=favicon.ico"


# route("/drive/list", methods=["GET"])
def test_show_named_drive_presets(http_client):
    response = http_client.get("/drive/list")
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "files" in response_data["data"]


# route("/drive/cdrom", methods=["POST"])
def test_create_cdrom_properties_file(env, http_client):
    file_name = f"{uuid.uuid4()}.iso"

    response = http_client.post(
        "/drive/cdrom",
        data={
            "drive_name": "Sony CDU-8012",
            "file_name": file_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == (
        f"File created: {env['cfg_dir']}/{file_name}.properties"
    )


# route("/drive/create", methods=["POST"])
def test_create_image_with_properties_file(http_client, delete_file):
    file_prefix = str(uuid.uuid4())
    file_name = f"{file_prefix}.hds"

    response = http_client.post(
        "/drive/create",
        data={
            "drive_name": "Miniscribe M8425",
            "size": FILE_SIZE_1_MIB,
            "file_name": file_prefix,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert (
        response_data["messages"][0]["message"]
        == f"Image file with properties created: {file_name}"
    )

    # Cleanup
    delete_file(file_name)


# route("/sys/manpage", methods=["POST"])
def test_show_manpage(http_client):
    response = http_client.get("/sys/manpage?app=rascsi")
    response_data = response.json()

    assert response.status_code == 200
    assert "rascsi" in response_data["data"]["manpage"]
