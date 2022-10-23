#!/usr/bin/env bash

if ! [[ -f "/home/pi/RASCSI/python/common/src/rascsi_interface_pb2.py" ]]; then
    # Build rascsi_interface_pb2.py with the protobuf compiler
    protoc \
        -I=/home/pi/RASCSI/src/raspberrypi \
        --python_out=/home/pi/RASCSI/python/common/src \
        rascsi_interface.proto
fi

# Start Nginx service
nginx

# Pass args to web UI start script
if [[ $RASCSI_PASSWORD ]]; then
    /home/pi/RASCSI/python/web/start.sh "$@" --password=$RASCSI_PASSWORD
else
    /home/pi/RASCSI/python/web/start.sh "$@"
fi
