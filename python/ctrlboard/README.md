# PiSCSI Control Board UI

## Run as standalone script for development / troubleshooting

```bash
# Make a virtual env named venv
$ python3 -m venv venv
# Use that virtual env in this shell
$ source venv/bin/activate
# Install requirements
$ pip3 install -r requirements.txt
$ python3 src/main.py
```

### Parameters

The script parameters can be shown with
```
python src/main.py --help
```
or
```
start.sh --help
```

Example:
```
$ python3 src/main.py --rotation=0 --transitions=0
```

## Run the start.sh script standalone

The start.sh script can also be run standalone, and will handle the venv creation/updating for you. It takes the same command line parameters in the following format:

```
$ ./start.sh --rotation=0 --transitions=0
```
### I2C baudrate and transitions
The available bandwidth for the display through I2C is limited. The I2C baudrate is automatically adjusted in
easy_install.sh and start.sh to maximize the possible baudrate, however, depending on the Raspberry Pi model in use, we found that for some
models enabling the transitions does not work very well. As a result, we have implemented a model detection for
the raspberry pi model and enable transitions only for the following pi models:
- Raspberry Pi 4 models
- Raspberry Pi 3 models
- Raspberry Pi Zero 2 models

The model detection can be overriden by adding a --transitions parameter to start.sh.
