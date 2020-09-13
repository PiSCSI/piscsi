# To use this, add the following to the end of your /home/<user>/.bashrc file
#
# if [ -f ~/RASCSI/src/developer_shortcuts.sh ]; then
#   source ~/RASCSI/src/developer_shortcuts.sh
# fi
#
alias rascsisrc='cd ~/RASCSI/src/raspberrypi'

alias rascsirebuild='cd ~/RASCSI/src/raspberrypi && sudo systemctl stop rascsi && make clean && make all DEBUG=1 -j && sudo make install && sudo systemctl start rascsi'

alias rascsirestart='sudo systemctl restart rascsi.service'
alias rascsistop='sudo systemctl stop rascsi.service'
alias rascsistart='sudo systemctl start rascsi.service'

# This command only works on a linux machine with a SCSI card.
alias scsirescan='echo "- - -" > /sys/class/scsi_host/host0/scan'
alias scsidelete='echo Reminder: the command to delete a SCSI device is: "echo 1 > /sys/class/scsi_device/h:c:t:l/device/delete"'


