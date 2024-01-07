import pytest
import uuid
import warnings
import datetime

SCSI_ID = 6
FILE_SIZE_1_MIB = 1048576
STATUS_SUCCESS = "success"
STATUS_ERROR = "error"

ENV_ENDPOINT = "/env"
HEALTHCHECK_ENDPOINT = "/healthcheck"
PWA_FAVICON_ENDPOINT = "/pwa/favicon.ico"
LOGIN_ENDPOINT = "/login"
LOGOUT_ENDPOINT = "/logout"
ATTACH_ENDPOINT = "/scsi/attach"
DETACH_ENDPOINT = "/scsi/detach"
DETACH_ALL_ENDPOINT = "/scsi/detach_all"
EJECT_ENDPOINT = "/scsi/eject"
RESERVE_ENDPOINT = "/scsi/reserve"
RELEASE_ENDPOINT = "/scsi/release"
INFO_ENDPOINT = "/scsi/info"
CREATE_ENDPOINT = "/files/create"
RENAME_ENDPOINT = "/files/rename"
COPY_ENDPOINT = "/files/copy"
DELETE_ENDPOINT = "/files/delete"
DOWNLOAD_URL_ENDPOINT = "/files/download_url"
DOWNLOAD_IMAGE_ENDPOINT = "/files/download_image"
DOWNLOAD_CONFIG_ENDPOINT = "/files/download_config"
EXTRACT_IMAGE_ENDPOINT = "/files/extract_image"
UPLOAD_ENDPOINT = "/files/upload"
CREATE_ISO_ENDPOINT = "/files/create_iso"
DISKINFO_ENDPOINT = "/files/diskinfo"
DRIVE_LIST_ENDPOINT = "/drive/list"
DRIVE_CREATE_ENDPOINT = "/drive/create"
DRIVE_CDROM_ENDPOINT = "/drive/cdrom"
MANPAGE_ENDPOINT = "/sys/manpage?app=piscsi"
LANGUAGE_ENDPOINT = "/language"
LOG_LEVEL_ENDPOINT = "/logs/level"
LOG_SHOW_ENDPOINT = "/logs/show"
CONFIG_SAVE_ENDPOINT = "/config/save"
CONFIG_ACTION_ENDPOINT = "/config/action"
THEME_ENDPOINT = "/theme"
SYS_RENAME_ENDPOINT = "/sys/rename"


@pytest.fixture(scope="function")
def create_test_image(request, http_client):
    images = []

    def create(image_type="hds", size=1, auto_delete=True):
        file_prefix = f"{request.function.__name__}___{uuid.uuid4()}"
        file_name = f"{file_prefix}.{image_type}"

        response = http_client.post(
            CREATE_ENDPOINT,
            data={
                "file_name": file_prefix,
                "type": image_type,
                "size": size,
            },
        )

        if response.json()["status"] != STATUS_SUCCESS:
            raise Exception("Failed to create temporary image")

        if auto_delete:
            images.append(
                {
                    "file_name": file_name,
                    "function": request.function,
                    "created": str(datetime.datetime.now()),
                }
            )

        return file_name

    def delete():
        for image in images:
            response = http_client.post(DELETE_ENDPOINT, data={"file_name": image["file_name"]})
            if response.status_code != 200 or response.json()["status"] != STATUS_SUCCESS:
                warnings.warn(
                    f"Failed to auto-delete file created with create_test_image fixture: {image}"
                )

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
        response = http_client.post(DELETE_ENDPOINT, data={"file_name": file_name})
        if response.status_code != 200 or response.json()["status"] != STATUS_SUCCESS:
            warnings.warn(f"Failed to delete file via delete_file fixture: {file_name}")

    return delete


@pytest.fixture(scope="function")
def detach_devices(http_client):
    def detach():
        response = http_client.post(DETACH_ALL_ENDPOINT)
        if response.json()["status"] == STATUS_SUCCESS:
            return True
        raise Exception("Failed to detach SCSI devices")

    return detach
