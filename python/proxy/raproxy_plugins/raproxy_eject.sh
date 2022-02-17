#!/bin/sh

scsi_id=$1
type=$2
path=$3

echo "shell script: ejecting from scsi id : " ${scsi_id}
echo "shell script: ejecting file type    : " ${type}
echo "shell script: ejecting file         : " ${path}

exit 0
