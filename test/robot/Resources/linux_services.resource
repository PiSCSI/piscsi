
*** Settings ***
Library         SSHLibrary
Library         String

*** Variables ***
${Rascsi_Host}      192.168.68.154
${Rascsi_Username}  pi
${Rascsi_Password}  raspberry

*** Keywords ***
The ${service_name} Service Should be Running
    ${lc_service_name}=  Convert To Lower Case  ${service_name}
    ${output}=  Execute Command  systemctl status ${lc_service_name}
    Should Contain  ${output}  Active: active (running)  ignore_case=True

The ${service_name} Service Should be Stopped
    ${lc_service_name}=  Convert To Lower Case  ${service_name}
    ${output}=  Execute Command  systemctl status ${lc_service_name}
    Should Contain  ${output}  Active: failed  ignore_case=True

The ${service_name} Service is Stopped
    ${lc_service_name}=  Convert To Lower Case  ${service_name}
    Execute Command  systemctl stop ${lc_service_name}  sudo=True  sudo_password=${Rascsi_Password}

The ${service_name} Service is Restarted
    ${lc_service_name}=  Convert To Lower Case  ${service_name}
    Execute Command  systemctl restart ${lc_service_name}  sudo=True  sudo_password=${Rascsi_Password}

The ${service_name} Service is Started
    ${lc_service_name}=  Convert To Lower Case  ${service_name}
    Execute Command  systemctl start ${lc_service_name}  sudo=True  sudo_password=${Rascsi_Password}
