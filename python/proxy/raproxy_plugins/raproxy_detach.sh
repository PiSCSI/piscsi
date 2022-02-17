#!/bin/sh

scsi_id=$1
type=$2
path=$3

echo "shell script: detaching from scsi id: " ${scsi_id}
echo "shell script: detaching file type   : " ${type}
echo "shell script: detaching file        : " ${path}

exit 0
