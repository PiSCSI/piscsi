![C/C++ CI](https://github.com/akuker/RASCSI/workflows/C/C++%20CI/badge.svg) [![Scrutinizer Code Quality](https://scrutinizer-ci.com/g/akuker/RASCSI/badges/quality-score.png?b=master)](https://scrutinizer-ci.com/g/akuker/RASCSI/?branch=master)
<a href="https://scan.coverity.com/projects/akuker-rascsi">
  <img alt="Coverity Scan Build Status"
       src="https://scan.coverity.com/projects/21656/badge.svg"/>
</a>

# What is RaSCSI?
RaSCSI is a virtual SCSI device emulator that runs on a Raspberry Pi. It runs in userspace, and can emulate several SCSI devices at one time. There is a control interface to attach / detach drives during runtime, as well as insert and eject removable media. This project is aimed at users of vintage Macintosh and Atari computers from the 1980's and 1990's.

Please check out the full story with much more detail on the [wiki](https://github.com/akuker/RASCSI/wiki)!

# How do I contribute?
RaSCSI is using the <a href="https://datasift.github.io/gitflow/IntroducingGitFlow.html">Gitflow Workflow</a>. A quick overview:

- The *master* branch should always reflect the contents of the last stable release
- The *develop* branch should contain the latest tested & approved updates. Pull requests should be used to merge changes into develop.
- The rest of the feature branches are for developing new features
- A tag will be created for each "release". The releases will be named <year>.<month> (for the first release of the month). Hot fixes, if necessary, will be released as <year>.<month>.<release number>. For example, the first release in January 2021 will be release "21.01". If a hot-fix is needed for this release, the first hotfix will be "21.01.1".
  
Typically, releases will only be planned every few months.

<a href="https://www.tindie.com/stores/landogriffin/?ref=offsite_badges&utm_source=sellers_akuker&utm_medium=badges&utm_campaign=badge_large"><img src="https://d2ss6ovg47m0r5.cloudfront.net/badges/tindie-larges.png" alt="I sell on Tindie" width="200" height="104"></a>
