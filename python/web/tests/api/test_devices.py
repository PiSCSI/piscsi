import pytest

from conftest import (
    SCSI_ID,
    FILE_SIZE_1_MIB,
    STATUS_SUCCESS,
)


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
        f"Attached {test_image} as Hard Disk Drive to SCSI ID {SCSI_ID} LUN 0"
    )

    # Cleanup
    detach_devices()


# route("/scsi/attach_device", methods=["POST"])
@pytest.mark.parametrize(
    "device_name,device_config",
    [
        (
            "Removable Disk Drive",
            {
                "type": "SCRM",
                "drive_props": {
                    "vendor": "VENDOR",
                    "product": "PRODUCT",
                    "revision": "0123",
                    "block_size": "512",
                },
            },
        ),
        (
            "Magneto-Optical Drive",
            {
                "type": "SCMO",
                "drive_props": {
                    "vendor": "VENDOR",
                    "product": "PRODUCT",
                    "revision": "0123",
                    "block_size": "512",
                },
            },
        ),
        (
            "CD/DVD Drive",
            {
                "type": "SCCD",
                "drive_props": {
                    "vendor": "VENDOR",
                    "product": "PRODUCT",
                    "revision": "0123",
                    "block_size": "512",
                },
            },
        ),
        ("Host Bridge", {"type": "SCBR", "interface": "eth0", "inet": "10.10.20.1/24"}),
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

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Detached SCSI ID {SCSI_ID} LUN 0"


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
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "devices" in response_data["data"]
    assert response_data["data"]["devices"][0]["file"] == test_image

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
