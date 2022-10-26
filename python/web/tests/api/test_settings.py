import pytest
import uuid

from conftest import STATUS_SUCCESS


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
@pytest.mark.parametrize("level", ["trace", "debug", "info", "warn", "err", "off"])
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
            "scope": "rascsi",
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["data"]["lines"] == "100"
    assert response_data["data"]["scope"] == "rascsi"


# route("/config/save", methods=["POST"])
# route("/config/load", methods=["POST"])
def test_save_load_and_delete_configs(env, http_client):
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
        f"File created: {env['cfg_dir']}/{config_json_file}"
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
        f"Loaded configurations from: {config_json_file}"
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
        f"File deleted: {env['cfg_dir']}/{config_json_file}"
    )

    assert config_json_file not in http_client.get("/").json()["data"]["config_files"]
