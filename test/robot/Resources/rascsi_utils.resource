
*** Settings ***
Library         SSHLibrary
Resource        linux_services.resource

*** Variables ***
${Rascsi_Host}      rascsi.local
${Rascsi_Username}  pi
${Rascsi_Password}  raspberry

*** Keywords ***
Clear login database
    Remove file    ${DATABASE FILE}

Open Connection to Rascsi and Log In
    Open Connection  ${Rascsi_Host}
    Login  ${Rascsi_Username}  ${Rascsi_Password}

Create valid user
    [Arguments]    ${username}    ${password}
    Create user    ${username}    ${password}
    Status should be    SUCCESS

Creating user with invalid password should fail
    [Arguments]    ${password}    ${error}
    Create user    example    ${password}
    Status should be    Creating user failed: ${error}
