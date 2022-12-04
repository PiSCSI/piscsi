#!/usr/bin/env bash

if ! [[ -f "/home/pi/piscsi/python/common/src/piscsi_interface_pb2.py" ]]; then
    # Build piscsi_interface_pb2.py with the protobuf compiler
    protoc \
        -I=/home/pi/piscsi/cpp \
        --python_out=/home/pi/piscsi/python/common/src \
        piscsi_interface.proto
fi

# Start Nginx service
nginx

# Use mock commands
export PATH="/home/pi/piscsi/python/web/mock/bin:$PATH"

# Pass args to web UI start script
if [[ $BACKEND_PASSWORD ]]; then
    /home/pi/piscsi/python/web/start.sh "$@" --password=$BACKEND_PASSWORD
else
    /home/pi/piscsi/python/web/start.sh "$@"
fi
