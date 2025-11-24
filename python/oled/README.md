# PiSCSI OLED Screen

## Run as standalone script for development / troubleshooting

```bash
# Make a virtual env named venv
$ python3 -m venv venv
# Use that virtual env in this shell
$ source venv/bin/activate
# Install requirements
$ pip3 install -r requirements.txt
$ PYTHONPATH=$PWD/src:$(dirname $PWD)/common/src python3 src/piscsi_oled_monitor.py
```

### Parameters

The script takes two positional parameters:
* '0' or '180' which decides the screen rotation
* '32' or '64' which decides the vertical screen resolution in pixels

Ex.
```
$ python3 piscsi_oled_monitor.py --rotation 180 --height 64
```

_Note:_ Both parameters must be passed for the script to read them. Ordering is also important.

## Run the start.sh script standalone

The start.sh script can also be run standalone, and will handle the venv creation/updating for you. It takes the same command line parameters in the following format:

```
$ ./start.sh --rotation=180 --height=64
```
