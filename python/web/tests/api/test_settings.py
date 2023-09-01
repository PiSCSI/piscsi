import pytest
import uuid

from conftest import STATUS_SUCCESS


# route("/language", methods=["POST"])
@pytest.mark.parametrize(
    "locale,confirm_message",
    [
        ("de", "Webinterface-Sprache auf Deutsch geändert"),
        ("es", "Se ha cambiado el lenguaje de la Interfaz Web a español"),
        ("fr", "Langue de l’interface web changée en français"),
        ("sv", "Bytte webbgränssnittets språk till svenska"),
        ("en", "Changed Web Interface language to English"),
        ("zh", "Web 界面语言已更改为 中文"),
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
            "scope": "piscsi",
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["data"]["lines"] == "100"
    assert response_data["data"]["scope"] == "piscsi"


# route("/config/save", methods=["POST"])
# route("/config/action", methods=["POST"])
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
        "/config/action",
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
        "/config/action",
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


# route("/config/save", methods=["POST"])
# route("/config/action", methods=["POST"])
def test_download_configs(env, http_client, delete_file):
    config_name = str(uuid.uuid4())
    config_json_file = f"{config_name}.json"

    # Save the initial state to a config
    response = http_client.post(
        "/config/save",
        data={
            "name": config_name,
        },
    )

    assert response.status_code == 200
    assert config_json_file in http_client.get("/").json()["data"]["config_files"]

    # Download the saved config
    response = http_client.post(
        "/config/action",
        data={
            "name": config_json_file,
            "send": True,
        },
    )

    assert response.status_code == 200
    assert response.headers["content-type"] == "application/json"
    assert response.headers["content-disposition"] == f"attachment; filename={config_json_file}"

    # Delete the saved config
    response = http_client.post(
        "/config/action",
        data={
            "name": config_json_file,
            "delete": True,
        },
    )

    assert response.status_code == 200
    assert config_json_file not in http_client.get("/").json()["data"]["config_files"]


# route("/theme", methods=["POST"])
@pytest.mark.parametrize(
    "theme",
    [
        "modern",
        "classic",
    ],
)
def test_set_theme(http_client, theme):
    response = http_client.post(
        "/theme",
        data={
            "name": theme,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Theme changed to '{theme}'."


# route("/theme", methods=["GET"])
@pytest.mark.parametrize(
    "theme",
    [
        "modern",
        "classic",
    ],
)
def test_set_theme_via_query_string(http_client, theme):
    response = http_client.get(
        "/theme",
        params={
            "name": theme,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Theme changed to '{theme}'."


# route("/sys/rename", methods=["POST"])
def test_rename_system(env, http_client):
    new_name = "SYSTEM NAME TEST"

    response = http_client.get("/env")
    response_data = response.json()

    old_name = response_data["data"]["system_name"]

    response = http_client.post(
        "/sys/rename",
        data={
            "system_name": new_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"System name changed to '{new_name}'."

    response = http_client.get("/env")
    response_data = response.json()

    assert response_data["data"]["system_name"] == new_name

    response = http_client.post(
        "/sys/rename",
        data={
            "system_name": old_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"System name changed to '{old_name}'."

    response = http_client.get("/env")
    response_data = response.json()

    assert response_data["data"]["system_name"] == old_name


def test_throttle_notification(http_client):
    response = http_client.get("/")
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "Under voltage detected" in response_data["data"]
