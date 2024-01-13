import pytest
import uuid

from conftest import (
    STATUS_SUCCESS,
    ENV_ENDPOINT,
    LANGUAGE_ENDPOINT,
    LOG_LEVEL_ENDPOINT,
    LOG_SHOW_ENDPOINT,
    CONFIG_SAVE_ENDPOINT,
    CONFIG_ACTION_ENDPOINT,
    THEME_ENDPOINT,
    SYS_RENAME_ENDPOINT,
    RESERVE_ENDPOINT,
)


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
        LANGUAGE_ENDPOINT,
        data={
            "locale": locale,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == confirm_message


@pytest.mark.parametrize("level", ["trace", "debug", "info", "warn", "err", "off"])
def test_set_log_level(http_client, level):
    response = http_client.post(
        LOG_LEVEL_ENDPOINT,
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
        LOG_LEVEL_ENDPOINT,
        data={
            "level": "debug",
        },
    )


def test_show_logs(http_client):
    response = http_client.post(
        LOG_SHOW_ENDPOINT,
        data={
            "lines": 100,
            "scope": "piscsi",
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["data"]["lines"] == "100"
    assert response_data["data"]["scope"] == "piscsi"


def test_save_load_and_delete_configs(env, http_client):
    config_name = str(uuid.uuid4())
    config_json_file = f"{config_name}.json"
    reserved_scsi_id = 0
    reservation_memo = str(uuid.uuid4())

    # Confirm the initial state
    assert http_client.get("/").json()["data"]["RESERVATIONS"][0] == ""

    # Save the initial state to a config
    response = http_client.post(
        CONFIG_SAVE_ENDPOINT,
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
        RESERVE_ENDPOINT,
        data={
            "scsi_id": reserved_scsi_id,
            "memo": reservation_memo,
        },
    )

    assert http_client.get("/").json()["data"]["RESERVATIONS"][0] == reservation_memo

    # Load the saved config
    response = http_client.post(
        CONFIG_ACTION_ENDPOINT,
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
        CONFIG_ACTION_ENDPOINT,
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


def test_download_configs(env, http_client):
    config_name = str(uuid.uuid4())
    config_json_file = f"{config_name}.json"

    # Save the initial state to a config
    response = http_client.post(
        CONFIG_SAVE_ENDPOINT,
        data={
            "name": config_name,
        },
    )

    assert response.status_code == 200
    assert config_json_file in http_client.get("/").json()["data"]["config_files"]

    # Download the saved config
    response = http_client.post(
        CONFIG_ACTION_ENDPOINT,
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
        CONFIG_ACTION_ENDPOINT,
        data={
            "name": config_json_file,
            "delete": True,
        },
    )

    assert response.status_code == 200
    assert config_json_file not in http_client.get("/").json()["data"]["config_files"]


@pytest.mark.parametrize(
    "theme",
    [
        "modern",
        "classic",
    ],
)
def test_set_theme(http_client, theme):
    response = http_client.post(
        THEME_ENDPOINT,
        data={
            "name": theme,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Theme changed to '{theme}'."


@pytest.mark.parametrize(
    "theme",
    [
        "modern",
        "classic",
    ],
)
def test_set_theme_via_query_string(http_client, theme):
    response = http_client.get(
        THEME_ENDPOINT,
        params={
            "name": theme,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Theme changed to '{theme}'."


def test_rename_system(env, http_client):
    new_name = "SYSTEM NAME TEST"

    response = http_client.get(ENV_ENDPOINT)
    response_data = response.json()

    old_name = response_data["data"]["system_name"]

    response = http_client.post(
        SYS_RENAME_ENDPOINT,
        data={
            "system_name": new_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"System name changed to '{new_name}'."

    response = http_client.get(ENV_ENDPOINT)
    response_data = response.json()

    assert response_data["data"]["system_name"] == new_name

    response = http_client.post(
        SYS_RENAME_ENDPOINT,
        data={
            "system_name": old_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"System name changed to '{old_name}'."

    response = http_client.get(ENV_ENDPOINT)
    response_data = response.json()

    assert response_data["data"]["system_name"] == old_name
