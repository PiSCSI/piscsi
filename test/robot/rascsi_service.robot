*** Settings ***
Documentation     Test that the RaSCSI service can be started and stopped.
Library           OperatingSystem
Resource          Resources/rascsi_utils.resource
Resource          Resources/linux_services.resource

Test Setup        Open Connection to Rascsi and Log In
Test Teardown     Close All Connections

*** Test Cases ***
RaSCSI Service Can be Started as a Service
    [Documentation]  Start the Rascsi service and make sure it stays running
    When The Rascsi Service is Started
    Then The RaSCSI Service Should be Running

RaSCSI Service Can be Stopped as a Service
    [Documentation]  Stop the Rascsi service and make sure it stays stopped
    When the Rascsi Service is Stopped
    Then the RaSCSI Service should be Stopped

RaSCSI Service Can be Restarted as a Service
    [Documentation]  Start the Rascsi service and make sure it stays running
    Given The RaSCSI Service is Started
    When The Rascsi Service is Restarted
    Then the Rascsi Service should be Running



##*** Keywords ***
##Database Should Contain
#    [Arguments]    ${username}    ${password}    ${status}
#    ${database} =     Get File    ${DATABASE FILE}
#    Should Contain    ${database}    ${username}\t${password}\t${status}\n