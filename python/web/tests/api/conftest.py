import pytest
import uuid
import warnings

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
            response = http_client.post("/files/delete", data={"file_name": image})
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
        response = http_client.post("/files/delete", data={"file_name": file_name})
        if response.status_code != 200 or response.json()["status"] != STATUS_SUCCESS:
            warnings.warn(f"Failed to delete file via delete_file fixture: {file_name}")

    return delete


@pytest.fixture(scope="function")
def detach_devices(http_client):
    def detach():
        response = http_client.post("/scsi/detach_all")
        if response.json()["status"] == STATUS_SUCCESS:
            return True
        raise Exception("Failed to detach SCSI devices")

    return detach
