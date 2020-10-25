# RaSCSI Hardware
A high lever overview of the RaSCSI versions is available on the wiki: https://github.com/akuker/RASCSI/wiki/Hardware-Versions

# Other things
* IIciCase - A 3D printable case, developed in FreeCAD, that resembles a Mac IIci case. In order to use this case, you'll need to take the following into consideration:
  * This case only works with a Raspberry Pi Zero, where it is mounted on TOP of the RaSCSI
  * When you assemble the RaSCSI, you should use plain header pins instead of the box header for the 50-pin SCSI connector. If you don't, the header will be too wide to fit in the case
* Daisy Chain Board - This board can stack on top of the RaSCSI to add a second SCSI port. This allows the RaSCSI to exist in the middle of the SCSI chain.
* RaSCSI Tester - A prototype board that was intended to test new RaSCSIs to make sure they're working properly. This board was never debugged. Contact akuker if you are interested in doing something with it!
