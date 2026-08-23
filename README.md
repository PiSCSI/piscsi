# What is PiSCSI?

[![Build Status](https://github.com/PiSCSI/piscsi/actions/workflows/cpp.yml/badge.svg)](https://github.com/PiSCSI/piscsi/actions/workflows/cpp.yml)
[![Project releases](https://img.shields.io/github/release/PiSCSI/piscsi)](https://github.com/PiSCSI/piscsi/releases)
[![Project contributors](https://img.shields.io/github/contributors/PiSCSI/piscsi)](https://github.com/PiSCSI/piscsi/graphs/contributors)
[![License: BSD 3-Clause](https://img.shields.io/github/license/PiSCSI/piscsi)](https://github.com/PiSCSI/piscsi/blob/develop/LICENSE)
[![Security](https://sonarcloud.io/api/project_badges/measure?project=akuker-PISCSI&metric=security_rating)](https://sonarcloud.io/project/overview?id=akuker-PISCSI)
[![Reliability](https://sonarcloud.io/api/project_badges/measure?project=akuker-PISCSI&metric=reliability_rating)](https://sonarcloud.io/project/overview?id=akuker-PISCSI)
[![Maintainability](https://sonarcloud.io/api/project_badges/measure?project=akuker-PISCSI&metric=sqale_rating)](https://sonarcloud.io/project/overview?id=akuker-PISCSI)

PiSCSI is a virtual SCSI device emulator that runs on a Raspberry Pi through the [PiSCSI hat](https://github.com/PiSCSI/piscsi/wiki/Hardware-Versions).
The software runs in userspace, and can emulate several SCSI devices at one time.
There is a control interface to attach / detach drives during runtime, as well as insert and eject removable media.

This project aims to be compliant with the SCSI specification and usable with any standards compliant SCSI controller (see [compatibility list](https://github.com/PiSCSI/piscsi/wiki/Compatibility)).
A handful of PiSCSI features were written specifically for vintage Macintosh and Atari computers and may depend on non-free device drivers on the host machine.

PiSCSI is a fork of [RaSCSI](https://github.com/RaSCSI/RaSCSI) and maintains compatibility with any RaSCSI hat.

Please check out the full story with much more detail on the [wiki](https://github.com/PiSCSI/piscsi/wiki)!

## How do I contribute?

PiSCSI is using a variant of the Gitflow Workflow. A quick overview:

- The *main* branch should always reflect the contents of the last stable release
- The *develop* branch should contain the latest tested & approved updates. Pull requests should be used to merge changes into develop.
- The rest of the feature branches are for developing new features
- A tag will be created for each "release". The releases will be named \<year\>.\<month\>.\<release number\> where the release number is incremented for each subsequent release tagged in the same calendar month. The first release of the month of January 2021 is called "21.01.01", the second one in the same month "21.01.02" and so on.

Note that the leading zeroes in the month and release number are used only in the git tags. The equivalent version number in the software is 21.1.1, 21.1.2, etc.

When you are ready to contribute code to PiSCSI, follow the [GitHub Forking and Pull Request workflow](https://docs.github.com/en/get-started/exploring-projects-on-github/contributing-to-a-project) to create your own fork where you can make changes, and then contribute it back to the project. Please remember to always make Pull Requests against the *develop* branch.

If you want to add a new translation, or improve upon an existing one, please follow the [Web Interface localization instructions](go/piscsi-web/README.md#updating-localizations). Once the translation is complete, please use the same workflow as above to contribute it to the project.

## GitHub Sponsors

Thank you to all of the GitHub sponsors who support the development community!

Extra special thank you to the Gold level sponsors!

- @mikelord68
- @SamplerSpa-dev

Special thank you to the Silver level sponsors!

- @stinkerton18
- @hsiboy
- @pendleton115
- @Teufelhunden-0311
- Private sponsor ;]

Thank you to Lin van der Slikke for the red panda logo design!
