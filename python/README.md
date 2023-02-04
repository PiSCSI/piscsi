# PiSCSI Python Apps 

This directory contains Python-based clients for PiSCSI as well as common 
packages that are shared among the clients. 

The following paragraphs in this README contain instructions that are shared 
among all Python apps.

## Supported Python interpreter

The policy in this project is to support the Python 3 interpreter that comes 
standard with the current stable, as well as previous stable release of Debian.

At the time of writing they are:
- Python 3.9.2 in [Debian Bullseye](https://packages.debian.org/bullseye/python3)
- Python 3.7.3 in [Debian Buster](https://packages.debian.org/buster/python3)

## Static analysis with pylint

It is recommended to run pylint against new code to protect against bugs
and keep the code readable and maintainable.
The local pylint configuration lives in .pylintrc.
In order for pylint to recognize venv libraries, the pylint-venv package is required.

```
sudo apt install pylint3
sudo pip install pylint-venv
source venv/bin/activate
pylint3 python_source_file.py
```

Examples:
```
# check a single file
pylint web/src/web.py

# check the python modules
pylint common/src
pylint web/src
pylint oled/src
```
