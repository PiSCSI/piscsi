
*** Settings ***
Library         SSHLibrary
Library         Process
Library         String
Resource        linux_services.resource

*** Variables ***
${Rascsi_Host}      rascsi.local
${Rascsi_Username}  pi
${Rascsi_Password}  raspberry
${Scsi_type_cdrom}  5

# Taken from https://en.wikipedia.org/wiki/SCSI_Peripheral_Device_Type
${Scsi_device_type_hard_drive}  0
${Scsi_device_type_magnetic_tape}  1
${Scsi_device_type_printer}  2
${Scsi_device_type_processor}  3
${Scsi_device_type_write_once_dev}  4
${Scsi_device_type_cd_rom}  5
${Scsi_device_type_scanner}  6
${Scsi_device_type_optical_memory}  7

*** Keywords ***
Open Connection to Rascsi and Log In
    Open Connection  ${Rascsi_Host}
    Login  ${Rascsi_Username}  ${Rascsi_Password}

Create Blank Rascsi Drive Image of Size ${drive_size} megabytes named ${file_name}
    [Documentation]  Creates an empty drive image on the Rascsi host of the specified size and file name
    Execute Command  dd if=/dev/zero of=/home/pi/images/${file_name} bs=1M count=${drive_size}

Delete drive image ${file_name}
    [Documentation]  Delete a disk drive image that was created on the raspberry pi
    Execute Command  rm /home/pi/images/${file_name}

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


Detach all SCSI Devices
    [Documentation]  Send detach commands for all of the SCSI IDs to make sure that there
    ...              aren't any left over before/after a test
    FOR    ${scsi_id}    IN RANGE    1    7
        Detach SCSI ID ${scsi_id}
    END

Detach SCSI ID ${scsi_id:\d+}
    [Documentation]  Detaches the specified SCSI ID from Rascsi
    Execute Command  rasctl -c detach -i ${scsi_id}

Rescan SCSI Bus
    [Documentation]  Commands Linux to re-scan the SCSI bus on the local host
    ${result}=  Run Process  sudo bash -c "echo \"- - -\" > /sys/class/scsi_host/host0/scan"  shell=True
    
Rasctl reports SCSI ID ${scsi_id} of type ${type:CD|MO|HD}
    [Documentation]  Executes rasctl and verifies that the drive is configured as the specified type
    ${rasctl_output}=  Execute Command  rasctl -l
    log  ${rasctl_output}
    Should Contain  ${rasctl_output}  |${SPACE*2}${scsi_id}${SPACE}|${SPACE*2}0${SPACE}|${SPACE}SC${type}${SPACE}|

SCSI ID ${scsi_id} reports vendor ${scsi_vendor}
    [Documentation]  Reads the SCSI vendor from sysfs and checks that it matches expected value
    #    Sleep  2 minutes
    ${read_scsi_info}=  OperatingSystem.Get File  /sys/class/scsi_device/0:0:${scsi_id}:0/device/vendor
    ${read_scsi_info}=  Remove String Using Regexp  ${read_scsi_info}  ${\n}    
    Log  Vendor ID read from sysfs was --${read_scsi_info}--
    Should Be Equal  ${read_scsi_info}  ${scsi_vendor}  Incorrect SCSI vendor reported for ID ${scsi_id}

SCSI ID ${scsi_id} reports revision ${scsi_rev}
    [Documentation]  Reads the SCSI vendor from sysfs and checks that it matches expected value
    ${read_scsi_info}=  OperatingSystem.Get File  /sys/class/scsi_device/0:0:${scsi_id}:0/device/rev
    ${read_scsi_info}=  Remove String Using Regexp  ${read_scsi_info}  ${\n}    
    Log  SCSI revision read from sysfs was ${read_scsi_info}
    Should Be Equal  ${read_scsi_info}  ${scsi_rev}  Incorrect SCSI version reported for ID ${scsi_id}

SCSI ID ${scsi_id} reports model ${scsi_model}
    [Documentation]  Reads the SCSI vendor from sysfs and checks that it matches expected value
    ${read_scsi_info}=  OperatingSystem.Get File  /sys/class/scsi_device/0:0:${scsi_id}:0/device/model
    ${read_scsi_info}=  Remove String Using Regexp  ${read_scsi_info}  ${\n}    
    Log  SCSI model read from sysfs was --${read_scsi_info}--
    Should Be Equal  ${read_scsi_info}  ${scsi_model}  Incorrect SCSI model reported for ID ${scsi_id}

SCSI ID ${scsi_id} reports type ${scsi_type}
    [Documentation]  Reads the SCSI type from sysfs and checks that it matches expected value
    ${read_scsi_info}=  OperatingSystem.Get File  /sys/class/scsi_device/0:0:${scsi_id}:0/device/type
    ${read_scsi_info}=  Remove String Using Regexp  ${read_scsi_info}  ${\n}    
    Log  SCSI model read from sysfs was ${read_scsi_info}
    Should Be Equal  ${read_scsi_info}  ${scsi_type}  Incorrect SCSI type reported for ID ${scsi_id}

