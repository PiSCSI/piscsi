
*** Settings ***
Library         String

*** Variables ***

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

Rescan SCSI Bus
    [Documentation]  Commands Linux to re-scan the SCSI bus on the local host
    ${result}=  Run  sudo bash -c "echo \\"- - -\\" > /sys/class/scsi_host/host0/scan"

SCSI ID ${scsi_id:\d+} is detected by Linux
    [Documentation]  Checks to see if a specific SCSI ID is found by lsscsi
    ${result}=  Run  lsscsi
    Log  ${result}
    Should Contain  ${result}  [0:0:${scsi_id}:0]

Delete all SCSI devices from Linux
    [Documentation]  Remove all SCSI IDs from the Linux SCSI driver
    FOR    ${scsi_id}    IN RANGE    0    7
        Delete SCSI ID ${scsi_id} from Linux
    END

Delete SCSI ID ${scsi_id:\d+} from Linux
    [Documentation]  Remove a SCSI ID from Linux. Basically, will un-do the Scan action
    Run  sudo bash -c "echo 1 > /sys/class/scsi_device/0:0:${scsi_id}:0/device/delete" 2>&1 > /dev/null
    
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

