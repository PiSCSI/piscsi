from conftest import STATUS_SUCCESS, STATUS_ERROR


# route("/login", methods=["POST"])
def test_login_with_valid_credentials(pytestconfig, http_client_unauthenticated):
    response = http_client_unauthenticated.post(
        "/login",
        data={
            "username": pytestconfig.getoption("rascsi_username"),
            "password": pytestconfig.getoption("rascsi_password"),
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "env" in response_data["data"]


# route("/login", methods=["POST"])
def test_login_with_invalid_credentials(http_client_unauthenticated):
    response = http_client_unauthenticated.post(
        "/login",
        data={
            "username": "__INVALID_USER__",
            "password": "__INVALID_PASS__",
        },
    )

    response_data = response.json()

    assert response.status_code == 401
    assert response_data["status"] == STATUS_ERROR
    assert response_data["messages"][0]["message"] == (
        "You must log in with valid credentials for a user in the 'rascsi' group"
    )


# route("/logout")
def test_logout(http_client):
    response = http_client.get("/logout")
    assert response.status_code == 200
