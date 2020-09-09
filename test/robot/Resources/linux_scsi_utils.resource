
*** Settings ***
Library         String

*** Variables ***
${drive_mount_path}  /mnt/temp_drive/

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
    ${lsscsi}=  Run  lsscsi
    log  ${lsscsi}

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

Checksum of Data File ${file_name} from mounted drive
    [Documentation]  Calculate the checksum of the specified file from the mounted drive
    ${sha_output}=  Run  sha512sum ${drive_mount_path}${file_name}
    Log  Checksum of the file was ${sha_output}
    @{sha_output_list}=  Split String  ${sha_output}  ${SPACE}
    ${sha_signature}=  Get From List  ${sha_output_list}  0
    [Return]  ${sha_signature}

Get device id for SCSI ID ${scsi_id:/d+}
    [Documentation]  Lookup the device name (ex /dev/sr0) for the specified SCSI ID
    ${lsscsi_output}=  Run  lsscsi --brief |grep \\\\[0\\\\:0\\\\:${scsi_id}\\\\:0\\\\] | cut -f 5 --delimiter=" "
    Should Match Regexp  ${lsscsi_output}  \/dev\/[\\w\\d]+  Unable to find device ID for SCSI ID ${scsi_id}
    [Return]  ${lsscsi_output}
    # log  ${lsscsi_output}
    # @{scsi_entries}=  Split To Lines  ${lsscsi_output}
    # FOR  ${cur_line}    IN    @{scsi_entries}
    #     Log  ${cur_line}
    #     @{scsi_line}=  SplitString  ${cur_line}
    #     ${linux_id}=  Set Variable  ${scsi_line}[0]
    #     ${device_id}=  Set Variable  ${scsi_line}[1]
    #     Return From Keyword If  "${linux_id}"=="[0:0:${scsi_id}:0]"  ${device_id}
    # END
    # [Return]  /dev/notfound

Get size of device id ${device_id} in bytes
    [Documentation]  Decypher the output of lsblk to determine the size of the
    ...              specified device ID
    ${bare_device_id}=  Remove String Using Regexp  ${device_id}  \/dev\/
    ${lsblk_output}=  Run  lsblk --raw --bytes | grep ${bare_device_id} | cut -f 4 --delimiter=" "
    [Return]  ${lsblk_output}

Get size of SCSI ID ${scsi_id} device in bytes
    [Documentation]  Given a SCSI ID, determine how big that drive is (in bytes)
    ${device_id}=  Get device id for SCSI ID ${scsi_id}
    ${device_size}=  Get size of device id ${device_id} in bytes
    [Return]  ${device_size}

Get checksum of SCSI ID ${scsi_id} device
    [Documentation]  Calculate the checksum of the disk image on the RaSCSI host
    ${device_id}=  Get device id for SCSI ID ${scsi_id}
    ${checksum}=  Run  md5sum ${device_id} | cut -f 1 --delimiter=" "
    [Return]  ${checksum}

Get filesystem type of SCSI ID ${scsi_id}
    [Documentation]  Get the type of file system for the specified scsi ID. It needs to be
    ...              mounted in order for this command to work
    ${device_id}=  Get device id for SCSI ID ${scsi_id}
    ${raw}=  Run  mount | grep ${device_id}
    log  ${raw}
    ${filesystem_type}=  Run  mount |grep ${device_id} |cut --delimiter=" " -f 5
    [Return]  ${filesystem_type}

The size of SCSI ID ${scsi_id} is equal to the size of ${image_name}
    [Documentation]  Get the size of the SCSI device specified and the size of the drive 
    ...              image and make sure they're equal
    ${device_size}=  Get size of SCSI ID ${scsi_id} device in bytes
    ${image_size}=  Get Size of RaSCSI Image ${image_name} in bytes
    Should be Equal  ${device_size}  ${image_size}  The device size did not match the image size

The checksum of SCSI ID ${scsi_id} is equal to the checksum of ${image_name}
    [Documentation]  Empty Keyword
    ${device_checksum}=  Get checksum of SCSI ID ${scsi_id} device
    ${image_checksum}=  Get checksum of RaSCSI Image ${image_name}
    Should be Equal  ${device_checksum}  ${image_checksum}  The device checksum did not match the image file

Mount SCSI ID ${scsi_id} as ${folder_name}
    [Documentation]  Mount the specified SCSI ID at the specified path in /mnt
    Run  sudo mkdir -p /mnt/${folder_name}
    ${device}=  Get device id for SCSI ID ${scsi_id}
    Run  sudo mount ${device} /mnt/${folder_name}
    ${df}=  Run  df -h
    Log  ${df}

SCSI ID ${scsi_id} has been mounted at ${folder_name}
    [Documentation]  Check that the 'df' utility reports that the specified SCSI ID is
    ...              mounted at the path specified
    ${device}=  Get device id for SCSI ID ${scsi_id}
    ${df_output}=  Run  mount | grep ${device} | cut --delimiter=" " -f 3
    log  ${df_output}

Unmount SCSI ID ${scsi_id}
    [Documentation]  Un-mount the drive specified. Currently, this doesn't use the drive ID. However,
    ...              it needs to be updated in the future to un-moune the correct device, instead of
    ...              of being hard-coded to a specific path.
    ${device}=  Get device id for SCSI ID ${scsi_id}
    Run  sudo umount ${device}


The file ${filename} matches the original in ${drive_image}
    And the file install/vmlinux matches the original in ubuntu-12.04.5-server-amd64.iso



The filesystem type of SCSI ID ${scsi_id} is ${fs_type}
    [Documentation]  Fetches the filesystem type of the specified SCSI ID and checks that it matches
    ${actual_fs_type}=  Get filesystem type of SCSI ID ${scsi_id}
    Should be Equal  ${fs_type}  ${actual_fs_type}  Incorrect file system type detected
