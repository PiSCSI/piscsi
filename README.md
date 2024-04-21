# What is PiSCSI?

[![Build Status](https://github.com/PiSCSI/piscsi/actions/workflows/cpp.yml/badge.svg)](https://github.com/PiSCSI/piscsi/actions/workflows/cpp.yml)
[![Project releases](https://img.shields.io/github/release/PiSCSI/piscsi)](https://github.com/PiSCSI/piscsi/releases)
[![Project contributors](https://img.shields.io/github/contributors/PiSCSI/piscsi)](https://github.com/PiSCSI/piscsi/graphs/contributors)
[![License: BSD 3-Clause](https://img.shields.io/github/license/PiSCSI/piscsi)](https://github.com/PiSCSI/piscsi/blob/develop/LICENSE)
[<img src="https://sonarcloud.io/images/project_badges/sonarcloud-orange.svg" height="20" />](https://sonarcloud.io/summary/new_code?id=akuker-PISCSI)

PiSCSI is a virtual SCSI device emulator that runs on a Raspberry Pi. It runs in userspace, and can emulate several SCSI devices at one time. There is a control interface to attach / detach drives during runtime, as well as insert and eject removable media. This project is aimed at users of vintage Macintosh and Atari computers and more (see [compatibility list](https://github.com/PiSCSI/piscsi/wiki/Compatibility)) from the 1980's and 1990's.

Please check out the full story with much more detail on the [wiki](https://github.com/PiSCSI/piscsi/wiki)!

# How do I contribute?

PiSCSI is using the <a href="https://datasift.github.io/gitflow/IntroducingGitFlow.html">Gitflow Workflow</a>. A quick overview:

- The *main* branch should always reflect the contents of the last stable release
- The *develop* branch should contain the latest tested & approved updates. Pull requests should be used to merge changes into develop.
- The rest of the feature branches are for developing new features
- A tag will be created for each "release". The releases will be named <year>.<month>.<release number> where the release number is incremented for each subsequent release tagged in the same calendar month. The first release of the month of January 2021 is called "21.01.01", the second one in the same month "21.01.02" and so on.
  
Typically, releases will only be planned every few months.

When you are ready to contribute code to PiSCSI, follow the <a href="https://docs.github.com/en/get-started/quickstart/contributing-to-projects">GitHub Forking and Pull Request workflow</a> to create your own fork where you can make changes, and then contribute it back to the project. Please remember to always make Pull Requests against the *develop* branch.

If you want to add a new translation, or improve upon an existing one, please follow the <a href="https://github.com/PiSCSI/piscsi/tree/master/python/web#localizing-the-web-interface">instructions in the Web Interface README</a>. Once the translation is complete, please use the same workflow as above to contribute it to the project.

# GitHub Sponsors

Thank you to all of the GitHub sponsors who support the development community!

 Special thank you to the Gold level sponsors!
  - <a href="https://github.com/mikelord68">@mikelord68</a>
  - <a href="https://github.com/SamplerSpa-de">@samplerspa-de</a>
  
Special thank you to the Silver level sponsors!
  - <a href="https://github.com/stinkerton18">@stinkerton18</a>
  - <a href="https://github.com/hsiboy">@hsiboy</a>
  - <a href="https://github.com/pendleton115">@pendleton115</a>
  - <a href="https://github.com/Teufelhunden-0311">@Teufelhunden-0311</a>
  - Private sponsor ;]
  
  Thank you to Lin van der Slikke for the red panda logo design!
