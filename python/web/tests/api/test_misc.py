import uuid

from conftest import (
    FILE_SIZE_1_MIB,
    STATUS_SUCCESS,
    ENV_ENDPOINT,
    PWA_FAVICON_ENDPOINT,
    HEALTHCHECK_ENDPOINT,
    DRIVE_LIST_ENDPOINT,
    DRIVE_CREATE_ENDPOINT,
    DRIVE_CDROM_ENDPOINT,
    MANPAGE_ENDPOINT,
)


def test_index(http_client):
    response = http_client.get("/")
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "devices" in response_data["data"]


def test_get_env_info(http_client):
    response = http_client.get(ENV_ENDPOINT)
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "running_env" in response_data["data"]


def test_pwa_route(http_client):
    response = http_client.get(PWA_FAVICON_ENDPOINT)

    assert response.status_code == 200
    assert response.headers["content-disposition"] == "inline; filename=favicon.ico"


def test_show_named_drive_presets(http_client):
    response = http_client.get(DRIVE_LIST_ENDPOINT)
    response_data = response.json()

    prev_drive = {"name": ""}
    for drive in (
        response_data["data"]["drive_properties"]["hd_conf"]
        + response_data["data"]["drive_properties"]["cd_conf"]
        + response_data["data"]["drive_properties"]["rm_conf"]
        + response_data["data"]["drive_properties"]["mo_conf"]
    ):
        # Test that the named drive has a name
        assert drive["name"] != ""
        # Test that "name" is unique for each named drive
        assert drive["name"] != prev_drive["name"]
        prev_drive = drive

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "files" in response_data["data"]


def test_create_cdrom_properties_file(env, http_client):
    file_name = f"{uuid.uuid4()}.iso"

    response = http_client.post(
        DRIVE_CDROM_ENDPOINT,
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


def test_create_image_with_properties_file(http_client, delete_file):
    file_prefix = str(uuid.uuid4())
    file_name = f"{file_prefix}.hds"

    response = http_client.post(
        DRIVE_CREATE_ENDPOINT,
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


def test_show_manpage(http_client):
    response = http_client.get(MANPAGE_ENDPOINT)
    response_data = response.json()

    assert response.status_code == 200
    assert "piscsi" in response_data["data"]["manpage"]


def test_healthcheck(http_client):
    response = http_client.get(HEALTHCHECK_ENDPOINT)
    assert response.status_code == 200
