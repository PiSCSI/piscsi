#!/usr/bin/env bash

if ! [[ -f "/home/pi/PISCSI/python/common/src/piscsi_interface_pb2.py" ]]; then
    # Build piscsi_interface_pb2.py with the protobuf compiler
    protoc \
        -I=/home/pi/PISCSI/src/raspberrypi \
        --python_out=/home/pi/PISCSI/python/common/src \
        piscsi_interface.proto
fi

# Start Nginx service
nginx

# Use mock commands
export PATH="/home/pi/PISCSI/python/web/mock/bin:$PATH"

# Pass args to web UI start script
if [[ $PISCSI_PASSWORD ]]; then
    /home/pi/PISCSI/python/web/start.sh "$@" --password=$PISCSI_PASSWORD
else
    /home/pi/PISCSI/python/web/start.sh "$@"
fi
