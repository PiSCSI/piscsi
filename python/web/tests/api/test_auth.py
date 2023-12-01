from conftest import STATUS_SUCCESS, STATUS_ERROR, LOGIN_ENDPOINT, LOGOUT_ENDPOINT


def test_login_with_valid_credentials(pytestconfig, http_client_unauthenticated):
    # Note: This test depends on the piscsi group existing and 'username' a member the group
    response = http_client_unauthenticated.post(
        LOGIN_ENDPOINT,
        data={
            "username": pytestconfig.getoption("piscsi_username"),
            "password": pytestconfig.getoption("piscsi_password"),
        },
    )

    response_data = response.json()

    assert response.status_code == 200
    assert response_data["status"] == STATUS_SUCCESS
    assert "env" in response_data["data"]


def test_login_with_invalid_credentials(http_client_unauthenticated):
    response = http_client_unauthenticated.post(
        LOGIN_ENDPOINT,
        data={
            "username": "__INVALID_USER__",
            "password": "__INVALID_PASS__",
        },
    )

    response_data = response.json()

    assert response.status_code == 401
    assert response_data["status"] == STATUS_ERROR
    assert response_data["messages"][0]["message"] == (
        "You must log in with valid credentials for a user in the 'piscsi' group"
    )


def test_logout(http_client):
    response = http_client.get(LOGOUT_ENDPOINT)
    assert response.status_code == 200
