# What is RaSCSI?
RaSCSI is a virtual SCSI device emulator that runs on a Raspberry Pi. It can emulate several SCSI devices at one time. There is a control interface to attach / detach drives during runtime, as well as insert and eject removable media. Simply launch the software, connect the RaSCSI interface board to your Mac or other SCSI computer and the devices will be recognized as physical SCSI devices!

The original RaSCSI project was done by GIMONS in Japanese for the Sharp X68000 (https://en.wikipedia.org/wiki/X68000).

GIMONS' version of the RaSCSI is fantastic, but has some limitations. For me, the primary issue was that all of his code and documentation is in Japanese. Since I don't speak or read Japanese, that limits the amount I can contribute to the project.

This project is a <b><i>fork</i></b> of GIMONS' RaSCSI project. As he releases updates, feel free to pull them into this fork. However, since the source code was translated to English, there are A LOT of differences if you try to compare them. This was forked from version 1.47 of GIMONS' project.

# Comparison
<table>
  <tr>
    <td> </td>
    <td><h2>GIMONS RaSCSI</h2></td>
    <td><h2>RaSCSI - 68kmla edition</h2></td>
  </tr>
  <tr>
    <td> </td>
    <td><a href=http://retropc.net/gimons/rascsi/>Homepage</a></td>
    <td><a href=https://github.com/akuker/RASCSI/wiki>Wiki</a><br>
      <a href=https://github.com/akuker/RASCSI>Github</a></td>
  </tr>
  <tr>
    <td>Code & Documentation</td>
    <td>Japanese</td>
    <td>English</td>
  </tr>
  <tr>
    <td>Primary hardware<br>target</td>
    <td><a href=https://en.wikipedia.org/wiki/X68000>Sharp X68000</a></td>
    <td><a href=https://en.wikipedia.org/wiki/Timeline_of_Macintosh_models>1980's and 1990's<br>SCSI Macintosh PCs</a></td>
  </tr>
</table>

# Who is XRayRobert?
<a href=https://github.com/XReyRobert>XReyRobert</a> was the first one to check in the RaSCSI source code into <a href=https://github.com/XReyRobert/RASCSI>Github</a>. It appears that he isn't actively working on RaSCSI. Even though this repo shows as a fork from XReyRobert, this one is based on a newer version of RaSCSI and has a TON of updates. 
  
# What is 68kmla?
68kmla is the <a href=https://wiki.68kmla.org/68kMLA:About>"Mac Liberation Army"</a>. Its a group of vintage Mac (and Apple) enthusiasts who talk about nerdy stuff on the forum. This development started as part of a <a href=https://68kmla.org/forums/index.php?app=forums&module=forums&controller=topic&id=30399>forum thread</a>. Stop by the forum and say Hi! (Seriously! We'd love to hear from you - what your rig is, etc)

# Want more info?
Read the <a href=https://github.com/akuker/RASCSI/wiki>wiki</a>!

# Other random info
A translated PDF of GIMON's website is avialable here:
https://github.com/akuker/RASCSI/blob/master/RASCSI_webpage_translated.pdf

## Links, links, links, links....
* Mirror of Mac OS 8 Documentation
** http://mirror.informatimago.com/next/developer.apple.com/documentation/macos8/mac8.html
* ATAPI list of SCSI commands:
** https://wiki.osdev.org/ATAPI
* Pioneer CD-ROM SCSI command set
** http://www.epicycle.org.uk/pioneercd/SCSI-2%20Command%20Set.pdf
* Seagate SCSI Commands Reference Manual (maybe too new to use as a reference?)
** https://origin-www.seagate.com/files/staticfiles/support/docs/manual/Interface%20manuals/100293068j.pdf
* Inside Macintosh "devices" mirror
** http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/Devices/Devices-2.html
* Mirror of Apple's original SCSI Manager documentation
** (OG SCSI Manager) http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/Devices/Devices-119.html
** (SCSI Manager 4.3) http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/Devices/Devices-151.html
* PDF versions of the SCSI Manager documentation
** (OG SCSI Manager)  https://developer.apple.com/library/archive/documentation/mac/pdf/Devices/SCSI_Manager.pdf
** (SCSI Manager 4.3)  https://developer.apple.com/library/archive/documentation/mac/pdf/Devices/SCSI_Manager_4.3.pdf
* Apple CD-ROM and Apple CD/DVD Driver Reference
** https://siber-sonic.com/mac/Vintage/CD_DVDdriver.html
* CD-ROM Drivers
** https://www.vintageapple.org/macdrivers/disk.shtml
* Inside Macintosh "Network Setup" mirror
** http://mirror.informatimago.com/next/developer.apple.com/documentation/macos8/pdf/NetworkSetup.pdf
* Inside Macintosh "Communications Toolbox" mirror
** http://mirror.informatimago.com/next/developer.apple.com/documentation/macos8/pdf/CommToolbox.pdf
* Network Services Location manager Developers Kit
** http://mirror.informatimago.com/next/developer.apple.com/documentation/macos8/pdf/NSL_Mgr.pdf
