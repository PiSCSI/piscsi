
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
    Remove File  /home/pi/images/tmp_*

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

# Create a ${iso_size} megabyte ISO named ${image_name} with random data
#     [Documentation]  Creates a file on the RaSCSI with random data (from /dev/urandom),
#     ...              then creates an ISO image with that data
#     Execute Command  mkdir /tmp/new_iso
#     Execute Command  dd if=/dev/urandom of=/tmp/new_iso/data.dat bs=1M count=${iso_size}
#     Execute Command  genisoimage -o /home/pi/images/${image_name} /tmp/new_iso/

Insert Removable Media ${image} into SCSI ID ${scsi_id}
    Execute Command  rasctl -c insert -i ${scsi_id} -f /home/pi/images/${image}
    ${result}=  Execute Command  rasctl -l
    Log  ${result}

Removable media ${image} is inserted into SCSI ID ${scsi_id:/d+}
    [Documentation]  Inserts the sepecified image name into the removable media drive
    Execute Command  rasctl -c insert -i ${scsi_id} -f /home/pi/images/${image}

# Insert removable media ${image_name} into SCSI ID ${scsi_id:/d+}
#     [Documentation]  Inserts the sepecified image name into the removable media drive
#     Execute Command  rasctl -c insert -i ${scsi_id} -f /home/pi/images/${image_name}

Eject removable media from SCSI ID ${scsi_id:/d+}
    [Documentation]  Inserts the sepecified image name into the removable media drive
    Execute Command  rasctl -c eject -i ${scsi_id}

Checksum of Random Data File from RaSCSI
    [Documentation]  Calcualte the SHA signature of the random data that was generated
    ...              on the RaSCSI host
    ${sha_output}=  Execute Command  sha512sum /tmp/new_iso/data.dat | cut -f 1
    Log  Checksum of the file was ${sha_output}
    [Return]  ${sha_output}

Get checksum of RaSCSI Image ${image_name}
    [Documentation]  Calculate the checksum of the disk image on the RaSCSI host
    ${checksum}=  Execute Command  md5sum /home/pi/images/${image_name} | cut -f 1 --delimiter=" "
    [Return]  ${checksum}

Get Size of RaSCSI Image ${image_name} in bytes
    [Documentation]  Return the size of the specified disk image in bytes
    ${size_output}=  Execute Command  du -b /home/pi/images/${image_name} | cut -f 1
    [Return]  ${size_output}


Get checksum of ${filename} from ISO ${image_name} on the RaSCSI Host
    [Documentation]  Extracts the specified file from the ISO, calculates the checksum
    ...              then returns that value
    ${temp_dirname}=  Generate Random String  10  [LETTERS]
    Execute Command  mkdir /tmp/${temp_dirname}  sudo=True  sudo_password=${Rascsi_Password}
    Execute Command  mount /home/pi/images/${image_name} /tmp/${temp_dirname}  sudo=True  sudo_password=${Rascsi_Password}
    ${checksum}=  Execute Command  md5sum /tmp/${temp_dirname}/${filename} | cut -f 1 --delimiter=" "
    Execute Command  umount /tmp/${temp_dirname}  sudo=True  sudo_password=${Rascsi_Password}
    Execute Command  rm -rf /tmp/${temp_dirname}  sudo=True  sudo_password=${Rascsi_Password}
    [Return]  ${checksum}

The RaSCSI service is configured to use port ${port_number}
    [Documentation]  Modifies the rascsi.service systemd configuration file to use the specified port
    ...              instead of the default. This will erase any other options that have been previously
    ...              set in your rascsi.service file.
    Execute command  sed -i 's/^ExecStart=.*/ExecStart=\\\/usr\\\/local\\\/bin\\\/rascsi -p ${port_number}/' /etc/systemd/system/rascsi.service  sudo=True  sudo_password=${Rascsi_Password}
    Execute command  systemctl daemon-reload  sudo=True  sudo_password=${Rascsi_Password}
    ${service_file}=  Execute command  cat /etc/systemd/system/rascsi.service
    log  ${service_file}

The RaSCSI service is configured to use default port
    [Documentation]  Restore the rascsi.service systemd configuration file to use the default rascsi
    ...              launch command
    Execute command  sed -i 's/^ExecStart=.*/ExecStart=\\\/usr\\\/local\\\/bin\\\/rascsi/' /etc/systemd/system/rascsi.service  sudo=True  sudo_password=${Rascsi_Password}
    Execute command  systemctl daemon-reload  sudo=True  sudo_password=${Rascsi_Password}
    ${service_file}=  Execute command  cat /etc/systemd/system/rascsi.service
    log  ${service_file}

RASCTL should connect on port ${port_number}
    ${stdout_out}  ${stderr_out}=  Execute Command  rasctl -l -p ${port_number}  return_stderr=True
    log  ${stdout_out}
    log  ${stderr_out}
    Should Contain  ${stdout_out}  No images currently attached

RASCTL should not connect on port ${port_number}
    ${stdout_out}  ${stderr_out}=  Execute Command  rasctl -l -p ${port_number}  return_stderr=True
    log  ${stdout_out}
    log  ${stderr_out}
    Should Contain  ${stderr_out}  Error: Can't connect to rascsi process on host

Kill all rascsi processes
    ${result}=  Execute command  killall -9 rascsi  sudo=True  sudo_password=${Rascsi_Password}
    log  ${result}