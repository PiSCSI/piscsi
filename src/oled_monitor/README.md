# RaSCSI OLED Screen

## Run as standalone script for development / troubleshooting

```bash
# Make a virtual env named venv
$ python3 -m venv venv
# Use that virtual env in this shell
$ source venv/bin/activate
# Install requirements
$ pip3 install -r requirements.txt
$ python3 rascsi_oled_monitor.py
```

### Parameters

The script takes two positional parameters:
* '0' or '180' which decides the screen rotation
* '32' or '64' which decides the vertical screen resolution in pixels

Ex.
```
$ python3 rascsi_oled_monitor.py 180 64
```

_Note:_ Both parameters must be passed for the script to read them. Ordering is also important.

## Run the start.sh script standalone

The start.sh script can also be run standalone, and will handle the venv creation/updating for you. It takes the same command line parameters in the following format:

```
$ ./start.sh --rotation=180 --height=64
```

## Static analysis with pylint

It is recommended to run pylint against new code to protect against bugs
and keep the code readable and maintainable.
The local pylint configuration lives in .pylintrc (symlink to ../.pylintrc)

```
$ sudo apt install pylint3
$ pylint3 python_source_file.py
```

## Credits
### type_writer.ttf
* _Type Writer_ TrueType font by Mandy Smith
* Source: https://www.dafont.com/type-writer.font
* Distributed under BSD 3-Clause by permission from author (see LICENSE for full text)

### splash_start_\*.bmp, splash_stop_\*.bmp
* Drawn by Daniel Markstedt
* Distributed under BSD 3-Clause by permission from author (see LICENSE for full text)
