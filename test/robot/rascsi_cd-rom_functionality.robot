*** Settings ***
Documentation     Test the RaSCSI CD-ROM emulation functionality.
Library           OperatingSystem
Resource          Resources/rascsi_utils.resource
Resource          Resources/linux_services.resource
Resource          Resources/linux_scsi_utils.resource

Suite Setup        Run Keywords  Open Connection to Rascsi and Log In
...                AND           The Rascsi Service is Started
...                AND           The RaSCSI Service Should be Running
...                AND           Detach all RaSCSI SCSI Devices
Suite Teardown     Run Keywords  Detach all RaSCSI SCSI Devices
...                AND           Delete all SCSI devices from Linux
...                AND           Close All Connections

Test Teardown      Run Keywords  Detach all RaSCSI SCSI Devices
...                AND           Delete all SCSI devices from Linux
...                AND           Delete all RaSCSI drive images

*** Test Cases ***
MacOS formated ISO is mounted and correct size is reported
    [Documentation]  Mount a MacOS formatted ISO and check that its size is
    ...              detected correctly. Note that Linux is not able to 
    ...              mount this image. Note: Marathon was chosen because
    ...              its a reasonably small image. Large images will make 
    ...              this test take a LONG time.
    Given CD-ROM Drive is attached as SCSI ID 6
    When Insert Removable Media marathon.iso into SCSI ID 6
    Then the size of SCSI ID 6 is equal to the size of marathon.iso
    And the checksum of SCSI ID 6 is equal to the checksum of marathon.iso

ISO-9660 formated ISO is mounted correct size is reported
    [Documentation]  Mount a pre-made IDS-9660 formatted ISO and check 
    ...              that it is read correctly
    [Teardown]  Unmount SCSI ID 5
    Given CD-ROM Drive is attached as SCSI ID 5
    When Insert Removable Media ubuntu-12.04.5-server-amd64.iso into SCSI ID 5
    And Mount SCSI ID 5 as ubuntu
    Then SCSI ID 5 has been mounted at ubuntu
    And the size of SCSI ID 5 is equal to the size of ubuntu-12.04.5-server-amd64.iso
    And the filesystem type of SCSI ID 5 is iso9660
    And the file pics/debian.jpg in the ubuntu directory matches the original in ISO ubuntu-12.04.5-server-amd64.iso
    And the file install/vmlinuz in the ubuntu directory matches the original in ISO ubuntu-12.04.5-server-amd64.iso

CD-ROM Read Speed is as fast as expected
    [Documentation]  Check that the read speed  from the emulated CD-ROM is within
    ...              the expected range. This should detect if a change cause the
    ...              drive to slow down significantly
    Given CD-ROM Drive is attached as SCSI ID 2
    When insert Removable Media ubuntu-12.04.5-server-amd64.iso into SCSI ID 2
    Then the measured read speed for 1 megabytes of data from SCSI ID 2 is greater than 750 KB/s
    Then the measured read speed for 10 megabytes of data from SCSI ID 2 is greater than 950 KB/s
    And the measured read speed for 100 megabytes of data from SCSI ID 2 is greater than 950 KB/s


Create an ISO from a RaSCSI CD-ROM device and verify it matches
    [Documentation]  On the RaSCSI, we'll mount an ISO, then on the host system, we
    ...              will generate a new ISO from the emulated CD-ROM and make sure
    ...              that it matches the original
    [Teardown]  Remove temporary file dup_simtower.iso
    Given CD-ROM Drive is attached as SCSI ID 0
    When Insert removable media simtower.iso into SCSI ID 0
    And create an image named dup_simtower.iso from SCSI ID 0
    Then local file dup_simtower.iso matches RaSCSI file simtower.iso

