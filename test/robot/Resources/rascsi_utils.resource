
*** Settings ***
Library         SSHLibrary
Library         Process
Library         String
Resource        linux_services.resource

*** Variables ***
${Rascsi_Host}      rascsi.local
${Rascsi_Username}  pi
${Rascsi_Password}  raspberry


*** Keywords ***
Open Connection to Rascsi and Log In
    Open Connection  ${Rascsi_Host}
    Login  ${Rascsi_Username}  ${Rascsi_Password}

Create Blank Rascsi Drive Image of Size ${drive_size} megabytes named ${file_name}
    [Documentation]  Creates an empty drive image on the Rascsi host of the specified size and file name
    Execute Command  dd if=/dev/zero of=/home/pi/images/${file_name} bs=1M count=${drive_size}

Delete drive image ${file_name}
    [Documentation]  Delete a disk drive image that was created on the raspberry pi
    Remove File  /home/pi/images/${file_name}

Delete all RaSCSI drive images
    [Documentation]  Delete all of the temporary drive images that were created on the RaSCSI host
    Remove File  /home/pi/images/*

Drive image ${drive_image_file} is attached as SCSI ID ${scsi_id}
    [Documentation]  Attaches an existing drive image to the RaSCSI
    ${rasctl_output}=  Execute Command  rasctl -i ${scsi_id} -c attach -f /home/pi/images/${drive_image_file}
    log  ${rasctl_output}
    ${rasctl_output}=  Execute Command  rasctl -l
    log  ${rasctl_output}
    Rescan SCSI Bus

CD-ROM drive is attached as SCSI ID ${scsi_id}
    [Documentation]  Attaches a CD-ROM device (without any media) to the RaSCSI
    ${rasctl_output}=  Execute Command  rasctl -i ${scsi_id} -c attach -t cd
    log  ${rasctl_output}
    ${rasctl_output}=  Execute Command  rasctl -l
    log  ${rasctl_output}
    Rescan SCSI Bus

Magneto Optical drive is attached as SCSI ID ${scsi_id}
    [Documentation]  Attaches a Magneto Optical device (without any media) to the RaSCSI
    Execute Command  rasctl -i ${scsi_id} -c attach -t mo
    ${rasctl_output}=  Execute Command  rasctl -l
    log  ${rasctl_output}
    Rescan SCSI Bus


Detach all RaSCSI SCSI Devices
    [Documentation]  Send detach commands for all of the SCSI IDs to make sure that there
    ...              aren't any left over before/after a test
    FOR    ${scsi_id}    IN RANGE    0    7
        Detach RaSCSI SCSI ID ${scsi_id}
    END

Detach RaSCSI SCSI ID ${scsi_id:\d+}
    [Documentation]  Detaches the specified SCSI ID from Rascsi
    Execute Command  rasctl -c detach -i ${scsi_id}

Rasctl reports SCSI ID ${scsi_id} of type ${type:CD|MO|HD}
    [Documentation]  Executes rasctl and verifies that the drive is configured as the specified type
    ${rasctl_output}=  Execute Command  rasctl -l
    log  ${rasctl_output}
    Should Contain  ${rasctl_output}  |${SPACE*2}${scsi_id}${SPACE}|${SPACE*2}0${SPACE}|${SPACE}SC${type}${SPACE}|
