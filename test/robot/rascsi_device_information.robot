*** Settings ***
Documentation     Test that the RaSCSI reports SCSI information correctly.
Library           OperatingSystem
Resource          Resources/rascsi_utils.resource
Resource          Resources/linux_services.resource

Suite Setup        Run Keywords  Open Connection to Rascsi and Log In
...                AND           The Rascsi Service is Started
...                AND           The RaSCSI Service Should be Running
...                AND           Detach all SCSI Devices
Suite Teardown     Close All Connections

*** Test Cases ***
Apple Hard Drive reports the correct device information
    [Documentation]  Create an empty Apple hard drive and verify that the SCSI
    ...              information is reported correctly to the Linux host
    [Setup]  Run Keyword and Ignore Error  Delete drive image apple_drive.hda
    [Teardown]  Run Keywords  Detach SCSI ID 0  Delete drive image apple_drive.hda
    Given Create Blank Rascsi Drive Image of Size 10 megabytes named apple_drive.hda
    When Drive image apple_drive.hda is attached as SCSI ID 0
    Then Rasctl reports SCSI ID 0 of type HD
    And SCSI ID 0 reports vendor ${SPACE}SEAGATE
    And SCSI ID 0 reports revision 0147
    And SCSI ID 0 reports model ${SPACE*10}ST225N
    And SCSI ID 0 reports type ${Scsi_device_type_hard_drive}

XM6 Hard Drive reports the correct device information
    [Documentation]  Create an empty XM6 hard drive and verify that the SCSI
    ...              information is reported correctly to the Linux host
    [Setup]  Run Keyword and Ignore Error  Delete drive image xm6_scsi_drive.hds
    [Teardown]  Run Keywords  Detach SCSI ID 1  Delete drive image xm6_scsi_drive.hds
    Given Create Blank Rascsi Drive Image of Size 10 megabytes named xm6_scsi_drive.hds
    When Drive image xm6_scsi_drive.hds is attached as SCSI ID 1
    Then Rasctl reports SCSI ID 1 of type HD
    And SCSI ID 1 reports vendor ${SPACE}SEAGATE
    And SCSI ID 1 reports revision 0147
    And SCSI ID 1 reports model ${SPACE*10}ST225N
    And SCSI ID 1 reports type ${Scsi_device_type_hard_drive}

NEC Hard Drive reports the correct device information
    [Documentation]  Create an empty NEC hard drive and verify that the SCSI
    ...              information is reported correctly to the Linux host
    [Setup]  Run Keyword and Ignore Error  Delete drive image nec_hard_drive.hdn
    [Teardown]  Run Keywords  Detach SCSI ID 2  Delete drive image nec_hard_drive.hdn
    Given Create Blank Rascsi Drive Image of Size 10 megabytes named nec_hard_drive.hdn
    When Drive image nec_hard_drive.hdn is attached as SCSI ID 2
    Then Rasctl reports SCSI ID 2 of type HD
    And SCSI ID 2 reports vendor NECCSI${SPACE*2}
    And SCSI ID 2 reports revision 0147
    And SCSI ID 2 reports model PRODRIVE LPS10S${SPACE}
    And SCSI ID 2 reports type ${Scsi_device_type_hard_drive}

Anex86 Hard Drive reports the correct device information
    [Documentation]  Create an empty Anex86 hard drive and verify that the SCSI
    ...              information is reported correctly to the Linux host
    [Setup]  Run Keyword and Ignore Error  Delete drive image anex86_hard_drive.hdi
    [Teardown]  Run Keywords  Detach SCSI ID 3  Delete drive image anex86_hard_drive.hdi
    Given Create Blank Rascsi Drive Image of Size 10 megabytes named anex86_hard_drive.hdi
    When Drive image anex86_hard_drive.hdi is attached as SCSI ID 3
    Then Rasctl reports SCSI ID 3 of type HD
    And SCSI ID 3 reports vendor RaSCSI${SPACE*2}
    And SCSI ID 3 reports revision 0147
    And SCSI ID 3 reports model CD-ROM CDU-55S${SPACE*2}
    And SCSI ID 3 reports type ${Scsi_device_type_hard_drive}

CD-ROM drive reports the correct device information
    [Documentation]  Create an CD-ROM drive with no media and verify that the SCSI
    ...              information is reported correctly to the Linux host
    [Teardown]  Detach SCSI ID 4
    When CD-ROM Drive is attached as SCSI ID 4
    Then Rasctl reports SCSI ID 4 of type CD
    And SCSI ID 4 reports vendor RaSCSI${SPACE*2}
    And SCSI ID 4 reports revision 0147
    And SCSI ID 4 reports model CD-ROM CDU-55S${SPACE*2}
    And SCSI ID 4 reports type ${Scsi_device_type_cd_rom}

Magneto Optical drive reports the correct device information
    [Documentation]  Create an Magneto Optical drive with no media and verify 
    ...              that the SCSI information is reported correctly to the 
    ...              Linux host
    [Teardown]  Detach SCSI ID 5
    When Magneto Optical drive is attached as SCSI ID 5
    Then Rasctl reports SCSI ID 5 of type MO
    And SCSI ID 5 reports vendor RaSCSI${SPACE*2}
    And SCSI ID 5 reports revision 0147
    And SCSI ID 5 reports model M2513A${SPACE*8}
    And SCSI ID 5 reports type ${Scsi_device_type_optical_memory}
