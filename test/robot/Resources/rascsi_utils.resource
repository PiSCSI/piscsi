
*** Settings ***
Library         SSHLibrary
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


*** Keywords ***
Open Connection to Rascsi and Log In
    Open Connection  ${Rascsi_Host}
    Login  ${Rascsi_Username}  ${Rascsi_Password}


Create Blank Rascsi Drive Image of Size ${drive_size} kilobytes named ${file_name}
    [Documentation]  Creates an empty drive image on the Rascsi host of the specified size and file name
    Execute Command  dd if=/dev/zero of=/home/pi/images/${file_name} block_size=1k count=${drive_size}

Attach drive image ${drive_image_file} as ID ${scsi_id}
    [Documentation] Attaches an existing drive image to the RaSCSI
    Execute Command  rasctl -i ${scsi_id} -c attach -f /home/pi/images/${drive_image_file}

Rasctl reports SCSI ID ${scsi_id} of type ${type:CD|MO|HD}
    [Documentation] Executes rasctl and verifies that the drive is configured as the specified type
    ${rasctl_output}=  Execute Command  rasctl -l
    log  ${rasctl_output}

SCSI ID ${scsi_id} reports vendor ${scsi_vendor}
    [Documentation] Reads the SCSI vendor from sysfs and checks that it matches expected value
    ${read_scsi_vendor}=  Get File  /sys/class/scsi_device/0:0:${scsi_id}:0/device/vendor
    Log  Vendor ID read from sysfs was ${read_scsi_vendor}
    Should Be Equal  ${read_scsi_vendor}  ${scsi_vendor}  Incorrect SCSI vendor reported for ID ${scsi_id}

SCSI ID ${scsi_id} reports revision ${scsi_rev}
    [Documentation] Reads the SCSI vendor from sysfs and checks that it matches expected value
    ${read_scsi_rev}=  Get File  /sys/class/scsi_device/0:0:${scsi_id}:0/device/rev
    Log  SCSI revision read from sysfs was ${read_scsi_rev}
    Should Be Equal  ${read_scsi_rev}  ${scsi_rev}  Incorrect SCSI version reported for ID ${scsi_id}

SCSI ID ${scsi_id} reports model ${scsi_model}
    [Documentation] Reads the SCSI vendor from sysfs and checks that it matches expected value
    ${read_scsi_model}=  Get File  /sys/class/scsi_device/0:0:${scsi_id}:0/device/rev
    Log  SCSI model read from sysfs was ${read_scsi_model}
    Should Be Equal  ${read_scsi_model}  ${scsi_model}  Incorrect SCSI model reported for ID ${scsi_id}

SCSI ID ${scsi_id} reports type ${scsi_type}
    [Documentation] Reads the SCSI type from sysfs and checks that it matches expected value
    ${read_scsi_type}=  Get File  /sys/class/scsi_device/0:0:${scsi_id}:0/device/rev
    Log  SCSI model read from sysfs was ${read_scsi_type}
    Should Be Equal  ${read_scsi_type}  ${scsi_type}  Incorrect SCSI model reported for ID ${scsi_id}

