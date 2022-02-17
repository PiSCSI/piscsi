#!/bin/sh

scsi_id=$1
type=$2
path=$3

echo "shell script: attaching from scsi id: " ${scsi_id}
echo "shell script: attaching file type   : " ${type}
echo "shell script: attaching file        : " ${path}

exit 0
