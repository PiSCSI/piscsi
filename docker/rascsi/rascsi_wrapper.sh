#!/usr/bin/env bash

if [[ $RASCSI_PASSWORD ]]; then
    TOKEN_FILE="/home/pi/.config/rascsi/rascsi_secret"
    mkdir -p /home/pi/.config/rascsi || true
    echo $RASCSI_PASSWORD > $TOKEN_FILE
    chmod 700 $TOKEN_FILE
    /usr/local/bin/rascsi "$@" -P $TOKEN_FILE
else
    /usr/local/bin/rascsi "$@"
fi
