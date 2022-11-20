import pytest
import requests
import socket
import os
from subprocess import run, CalledProcessError


def pytest_addoption(parser):
    default_base_url = "http://rascsi_web" if os.getenv("DOCKER") else "http://localhost:8080"

    parser.addoption("--home_dir", action="store", default="/home/pi")
    parser.addoption("--base_url", action="store", default=default_base_url)
    parser.addoption("--httpserver_host", action="store", default=socket.gethostname())
    parser.addoption("--httpserver_listen_address", action="store", default="0.0.0.0")
    parser.addoption("--rascsi_username", action="store", default="pi")
    parser.addoption("--rascsi_password", action="store", default="rascsi")


@pytest.fixture(scope="session")
def env(pytestconfig):
    home_dir = pytestconfig.getoption("home_dir")
    return {
        "is_docker": bool(os.getenv("DOCKER")),
        "home_dir": home_dir,
        "cfg_dir": f"{home_dir}/.config/rascsi",
        "images_dir": f"{home_dir}/images",
        "file_server_dir": f"{home_dir}/shared_files",
    }


@pytest.fixture(scope="session")
def httpserver_listen_address(pytestconfig):
    return (pytestconfig.getoption("httpserver_listen_address"), 0)


@pytest.fixture(scope="function", autouse=True)
def set_httpserver_hostname(pytestconfig, httpserver):
    # We need httpserver.url_for() to generate URLs pointing to the correct host
    httpserver.host = pytestconfig.getoption("httpserver_host")


@pytest.fixture(scope="session", autouse=True)
def ensure_all_devices_detached(create_http_client):
    http_client = create_http_client()
    http_client.post("/scsi/detach_all")


@pytest.fixture(scope="session")
def create_http_client(pytestconfig):
    def create(authenticate=True):
        session = requests.Session()
        session.headers.update({"Accept": "application/json"})
        session.original_request = session.request

        def relative_request(method, url, *args, **kwargs):
            if url[:4] != "http":
                url = pytestconfig.getoption("base_url") + url

            return session.original_request(method, url, *args, **kwargs)

        session.request = relative_request

        if authenticate:
            session.post(
                "/login",
                data={
                    "username": pytestconfig.getoption("rascsi_username"),
                    "password": pytestconfig.getoption("rascsi_password"),
                },
            )
        return session

    return create


@pytest.fixture(scope="function")
def http_client(create_http_client):
    return create_http_client(authenticate=True)


@pytest.fixture(scope="function")
def http_client_unauthenticated(create_http_client):
    return create_http_client(authenticate=False)
