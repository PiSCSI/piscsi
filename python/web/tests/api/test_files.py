import pytest
import uuid
import os

from conftest import (
    FILE_SIZE_1_MIB,
    STATUS_SUCCESS,
)


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


# route("/files/create", methods=["POST"])
def test_create_file_with_properties(http_client, list_files, delete_file):
    file_prefix = str(uuid.uuid4())
    file_name = f"{file_prefix}.hds"

    response = http_client.post(
        "/files/create",
        data={
            "file_name": file_prefix,
            "type": "hds",
            "size": 1,
            "drive_name": "DEC RZ22",
        },
    )

    response_data = response.json()

    assert response.status_code == 201
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["data"]["image"] == file_name
    assert (
        response_data["messages"][0]["message"]
        == f"Image file with properties created: {file_name}"
    )
    assert file_name in list_files()

    # Cleanup
    delete_file(file_name)


# route("/files/create", methods=["POST"])
def test_create_file_and_format_hfs(http_client, list_files, delete_file):
    file_prefix = str(uuid.uuid4())
    file_name = f"{file_prefix}.hda"

    response = http_client.post(
        "/files/create",
        data={
            "file_name": file_prefix,
            "type": "hda",
            "size": 1,
            "drive_format": "Lido 7.56",
        },
    )

    response_data = response.json()

    assert response.status_code == 201
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["data"]["image"] == file_name
    assert response_data["messages"][0]["message"] == f"Image file created: {file_name} (Lido 7.56)"
    assert file_name in list_files()

    # Cleanup
    delete_file(file_name)


# route("/files/create", methods=["POST"])
def test_create_file_and_format_fat(env, http_client, list_files, delete_file):
    if env["is_docker"]:
        pytest.skip("Test not supported in Docker environment.")
    file_prefix = str(uuid.uuid4())
    file_name = f"{file_prefix}.hdr"

    response = http_client.post(
        "/files/create",
        data={
            "file_name": file_prefix,
            "type": "hdr",
            "size": 1,
            "drive_format": "FAT32",
        },
    )

    response_data = response.json()

    assert response.status_code == 201
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["data"]["image"] == file_name
    assert response_data["messages"][0]["message"] == f"Image file created: {file_name} (FAT32)"
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
    file_name = create_test_image(auto_delete=False)

    response = http_client.post("/files/delete", data={"file_name": file_name})

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == f"Image file deleted: {file_name}"
    assert file_name not in list_files()


# route("/files/extract_image", methods=["POST"])
@pytest.mark.parametrize(
    "archive_file_name,image_file_name",
    [
        ("test_image.zip", "test_image_from_zip.hds"),
        ("test_image.sit", "test_image_from_sit.hds"),
        ("test_image.7z", "test_image_from_7z.hds"),
    ],
)
def test_extract_file(
    httpserver, http_client, list_files, delete_file, archive_file_name, image_file_name
):
    http_path = f"/images/{archive_file_name}"
    url = httpserver.url_for(http_path)

    with open(f"tests/assets/{archive_file_name}", mode="rb") as file:
        zip_file_data = file.read()

    httpserver.expect_request(http_path).respond_with_data(
        zip_file_data,
        mimetype="application/octet-stream",
    )

    http_client.post(
        "/files/download_url",
        data={
            "destination": "disk_images",
            "images_subdir": "",
            "url": url,
        },
    )

    response = http_client.post(
        "/files/extract_image",
        data={
            "archive_file": archive_file_name,
            "archive_members": image_file_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["messages"][0]["message"] == "Extracted 1 file(s)"
    assert image_file_name in list_files()

    # Cleanup
    delete_file(archive_file_name)
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
                "destination": "disk_images",
                "images_subdir": "",
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


# route("/files/download_image", methods=["POST"])
def test_download_image(http_client, create_test_image):
    file_name = create_test_image()

    response = http_client.post("/files/download_image", data={"file": file_name})

    assert response.status_code == 200
    assert response.headers["content-type"] == "application/octet-stream"
    assert response.headers["content-disposition"] == f"attachment; filename={file_name}"
    assert response.headers["content-length"] == str(FILE_SIZE_1_MIB)


# route("/files/download_config", methods=["POST"])
def test_download_properties(http_client, list_files, delete_file):
    file_prefix = str(uuid.uuid4())
    file_name = f"{file_prefix}.hds"

    response = http_client.post(
        "/files/create",
        data={
            "file_name": file_prefix,
            "type": "hds",
            "size": 1,
            "drive_name": "Miniscribe M8425",
        },
    )

    response_data = response.json()

    assert response.status_code == 201
    assert response_data["status"] == STATUS_SUCCESS
    assert response_data["data"]["image"] == file_name
    assert (
        response_data["messages"][0]["message"]
        == f"Image file with properties created: {file_name}"
    )
    assert file_name in list_files()

    response = http_client.post("/files/download_config", data={"file": f"{file_name}.properties"})

    assert response.status_code == 200
    assert response.headers["content-type"] == "application/octet-stream"
    assert response.headers["content-disposition"] == f"attachment; filename={file_name}.properties"

    # Cleanup
    delete_file(file_name)


# route("/files/download_url", methods=["POST"])
def test_download_url_to_dir(env, httpserver, http_client, list_files, delete_file):
    file_name = str(uuid.uuid4())
    http_path = f"/images/{file_name}"
    subdir = ""
    url = httpserver.url_for(http_path)

    with open("tests/assets/test_image.hds", mode="rb") as file:
        test_file_data = file.read()

    httpserver.expect_request(http_path).respond_with_data(
        test_file_data,
        mimetype="application/octet-stream",
    )

    response = http_client.post(
        "/files/download_url",
        data={
            "destination": "disk_images",
            "images_subdir": subdir,
            "url": url,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert file_name in list_files()
    assert (
        response_data["messages"][0]["message"]
        == f"Downloaded file to {env['images_dir']}/{subdir}{file_name}"
    )

    # Cleanup
    delete_file(file_name)


# route("/files/create_iso", methods=["POST"])
def test_create_iso_from_url(
    httpserver,
    http_client,
    list_files,
    delete_file,
):
    test_file_name = str(uuid.uuid4())
    iso_file_name = f"{test_file_name}.iso"

    http_path = f"/images/{test_file_name}"
    url = httpserver.url_for(http_path)
    ISO_TYPE = "ISO-9660 Level 1"

    with open("tests/assets/test_image.hds", mode="rb") as file:
        test_file_data = file.read()

    httpserver.expect_request(http_path).respond_with_data(
        test_file_data,
        mimetype="application/octet-stream",
    )

    response = http_client.post(
        "/files/create_iso",
        data={
            "type": ISO_TYPE,
            "url": url,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert iso_file_name in list_files()

    assert (
        response_data["messages"][0]["message"]
        == f"CD-ROM image {iso_file_name} with type {ISO_TYPE} was created."
    )

    # Cleanup
    delete_file(iso_file_name)


# route("/files/create_iso", methods=["POST"])
def test_create_iso_from_local_file(
    http_client,
    create_test_image,
    list_files,
    delete_file,
):
    test_file_name = create_test_image()
    iso_file_name = f"{test_file_name}.iso"

    ISO_TYPE = "HFS"

    response = http_client.post(
        "/files/create_iso",
        data={
            "type": ISO_TYPE,
            "file": test_file_name,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert iso_file_name in list_files()

    assert (
        response_data["messages"][0]["message"]
        == f"CD-ROM image {iso_file_name} with type {ISO_TYPE} was created."
    )

    # Cleanup
    delete_file(iso_file_name)


# route("/files/diskinfo", methods=["POST"])
def test_show_diskinfo(http_client, create_test_image):
    test_image = create_test_image()

    response = http_client.post(
        "/files/diskinfo",
        data={
            "file_name": test_image,
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert "Regular file" in response_data["data"]["diskinfo"]
