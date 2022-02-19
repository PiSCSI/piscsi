# RaSCSI Control Board UI

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

The script supports the following optional parameters:
* --rotation: '0','90','180' or '270' which decides the screen rotation. Default: 0
* --height: '64' which decides the vertical screen resolution in pixels. Default: 64
* --rascsi-host: hostname where the RaSCSI protobuf interface is accessed. Default: localhost
* --rascsi-port: port on which the RaSCSI protobuf interface is accessed. Default: 6868
* --password: Token password for RaSCSI authentication. Default: none
* --loglevel: 0 (notset), 10 (debug), 30 (warning), 40 (error), 50 (critical). Default: 30
* --transitions: 0 (disabled), 1 (enabled). Default: 1

Example
```
$ python3 src/main.py --rotation 0 --transitions 0
```

## Run the start.sh script standalone

The start.sh script can also be run standalone, and will handle the venv creation/updating for you. It takes the same command line parameters in the following format:

```
$ ./start.sh --rotation 0 --transitions 0
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

## Credits
### DejaVuSansMono-Bold.ttf
* Source: https://dejavu-fonts.github.io
* Distributed under DejaVu Fonts Lience (see DejaVu Fonts License.txt for full text)

### splash_start_\*.bmp, splash_stop_\*.bmp
* Drawn by Daniel Markstedt
* Distributed under BSD 3-Clause by permission from author (see LICENSE for full text)

### pca9554.py
* Source: https://github.com/IRNAS/pca9554-python
