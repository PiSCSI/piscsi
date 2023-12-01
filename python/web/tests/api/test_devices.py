import pytest

from conftest import (
    SCSI_ID,
    STATUS_SUCCESS,
    ATTACH_ENDPOINT,
    DETACH_ENDPOINT,
    DETACH_ALL_ENDPOINT,
    EJECT_ENDPOINT,
    RESERVE_ENDPOINT,
    RELEASE_ENDPOINT,
    INFO_ENDPOINT,
)


def test_attach_device_with_image(http_client, create_test_image, detach_devices):
    test_image = create_test_image()

    response = http_client.post(
        ATTACH_ENDPOINT,
        data={
            "file_name": test_image,
            "scsi_id": SCSI_ID,
            "unit": 0,
            "type": "SCHD",
        },
    )
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == (
        f"Attached Hard Disk Drive to SCSI ID {SCSI_ID} LUN 0"
    )

    # Cleanup
    detach_devices()


@pytest.mark.parametrize(
    "device_name,device_config",
    [
        (
            "Removable Disk Drive",
            {
                "type": "SCRM",
                "drive_props": {
                    "vendor": "HD VENDOR",
                    "product": "HD PRODUCT",
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
                    "vendor": "MO VENDOR",
                    "product": "MO PRODUCT",
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
                    "vendor": "CD VENDOR",
                    "product": "CD PRODUCT",
                    "revision": "0123",
                    "block_size": "512",
                },
            },
        ),
        # TODO: Find a portable way to detect network interfaces for testing
        ("Host Bridge", {"type": "SCBR", "param_inet": "192.168.0.1/24"}),
        # TODO: Find a portable way to detect network interfaces for testing
        ("Ethernet Adapter", {"type": "SCDP", "param_inet": "192.168.0.1/24"}),
        ("Host Services", {"type": "SCHS"}),
        ("Printer", {"type": "SCLP", "param_timeout": 60, "param_cmd": "lp -fart %f"}),
    ],
)
def test_attach_device(env, http_client, detach_devices, device_name, device_config):
    if env["is_docker"] and device_name == "Host Bridge":
        pytest.skip("Test not supported in Docker environment.")

    device_config["scsi_id"] = SCSI_ID
    device_config["unit"] = 0

    response = http_client.post(
        ATTACH_ENDPOINT,
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


def test_detach_device(http_client, create_test_image):
    test_image = create_test_image()

    http_client.post(
        ATTACH_ENDPOINT,
        data={
            "file_name": test_image,
            "scsi_id": SCSI_ID,
            "unit": 0,
            "type": "SCHD",
        },
    )

    response = http_client.post(
        DETACH_ENDPOINT,
        data={
            "scsi_id": SCSI_ID,
            "unit": 0,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Detached SCSI ID {SCSI_ID} LUN 0"


def test_detach_all_devices(http_client, create_test_image, list_attached_images):
    test_images = []
    scsi_ids = [4, 5, 6]

    for scsi_id in scsi_ids:
        test_image = create_test_image()
        test_images.append(test_image)

        http_client.post(
            ATTACH_ENDPOINT,
            data={
                "file_name": test_image,
                "scsi_id": scsi_id,
                "unit": 0,
                "type": "SCHD",
            },
        )

    assert list_attached_images() == test_images

    response = http_client.post(DETACH_ALL_ENDPOINT)
    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert list_attached_images() == []


def test_eject_device(http_client, create_test_image, detach_devices):
    test_image = create_test_image()

    http_client.post(
        ATTACH_ENDPOINT,
        data={
            "file_name": test_image,
            "scsi_id": SCSI_ID,
            "unit": 0,
            "type": "SCCD",  # CD-ROM
        },
    )

    response = http_client.post(
        EJECT_ENDPOINT,
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


def test_show_device_info(http_client, create_test_image, detach_devices):
    test_image = create_test_image()

    http_client.post(
        ATTACH_ENDPOINT,
        data={
            "file_name": test_image,
            "scsi_id": SCSI_ID,
            "unit": 0,
            "type": "SCHD",
        },
    )

    response = http_client.post(
        INFO_ENDPOINT,
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "devices" in response_data["data"]
    assert response_data["data"]["devices"][0]["file"] == test_image

    # Cleanup
    detach_devices()


def test_reserve_and_release_device(http_client):
    scsi_id = 0

    response = http_client.post(
        RESERVE_ENDPOINT,
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
        RELEASE_ENDPOINT,
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
