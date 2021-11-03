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

The script takes one positional parameter: '0' or '180' which decides the screen rotation

## Static analysis with pylint

It is recommended to run pylint against new code to protect against bugs
and keep the code readable and maintainable.
The local pylint configuration lives in .pylintrc (symlink to ../.pylintrc)

```
$ sudo apt install pylint3
$ pylint3 python_source_file.py
```

## Credits
type_writer.ttf
 "Type Writer" TrueType font by Mandy Smith
 Source: https://www.dafont.com/type-writer.font
 Distributed under BSD 3-Clause by permission from author (see LICENSE for full text)

splash_start.bmp, splash_stop.bmp
 Splash screen bitmap images
 Drawn by Daniel Markstedt
 Distributed under BSD 3-Clause license
