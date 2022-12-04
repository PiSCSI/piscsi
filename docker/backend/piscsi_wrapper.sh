#!/usr/bin/env bash

if [[ $BACKEND_PASSWORD ]]; then
    TOKEN_FILE="/home/pi/.config/piscsi/piscsi_secret"
    mkdir -p /home/pi/.config/piscsi || true
    echo $BACKEND_PASSWORD > $TOKEN_FILE
    chmod 700 $TOKEN_FILE
    /usr/local/bin/piscsi "$@" -P $TOKEN_FILE
else
    /usr/local/bin/piscsi "$@"
fi
