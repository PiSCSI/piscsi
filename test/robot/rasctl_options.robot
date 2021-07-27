*** Settings ***
Documentation     Test that the RaSCSI can be started on a configurable TCP port.
Library           OperatingSystem
Resource          Resources/rascsi_utils.resource
Resource          Resources/linux_services.resource

Suite Setup        Open Connection to Rascsi and Log In
Suite Teardown     Close All Connections

*** Test Cases ***
RASCTL Works on the Default Port
    [Documentation]  Check that RaSCSI starts with the default port
    [Teardown]  Run Keywords  Kill all rascsi processes  The Rascsi service is Restarted
    When The RaSCSI service is configured to use default port
    And The RaSCSI service is Restarted
    Then rasctl should connect on port 6868
    And rasctl should not connect on port 9999

RASCTL Works on User Specified Port 9999
    [Documentation]  Check that RaSCSI works with a user specified port "9999"
    [Teardown]  Run Keywords  The RaSCSI service is configured to use default port  The Rascsi service is Restarted
    When The RaSCSI service is configured to use port 9999
    And The RaSCSI service is Restarted
    Then rasctl should connect on port 9999
    And rasctl should not connect on port 6868

RASCTL Works on User Specified Port 6869
    [Documentation]  Check that RaSCSI works with a user specified port DEFAULT + 1
    [Teardown]  Run Keywords  The RaSCSI service is configured to use default port  The Rascsi service is Restarted
    When The RaSCSI service is configured to use port 6869
    And The RaSCSI service is Restarted
    Then rasctl should connect on port 6869
    And rasctl should not connect on port 6868
