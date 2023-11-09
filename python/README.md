# PiSCSI Python Apps 

This directory contains Python-based clients for PiSCSI as well as common 
packages that are shared among the clients. 

The following paragraphs in this README contain instructions that are shared 
among all Python apps.

## Supported Python interpreters

The policy in this project is to support the Python 3 interpreter that comes 
standard with the current stable, as well as the previous stable releases of Debian.

At the time of writing they are:
- Python 3.11 in [Debian Bookworm](https://packages.debian.org/bookworm/python3)
- Python 3.9 in [Debian Bullseye](https://packages.debian.org/bullseye/python3)

## Dependencies

We use 'pip freeze' to manage explicit Python dependencies in this project.
After adding new or bumping the versions of Python dependencies,
please run the following command in the requisite subdir commit the results:

```
pip freeze -l > requirements.txt
```

## Static analysis and formatting

The CI workflow is set up to check code formatting with `black`,
and linting with `flake8`. If non-conformant code is found, the CI job
will fail.

Before checking in new code, install the development packages and run
these two tools locally.

```
pip install -r web/requirements-dev.txt
```

Note that `black` only works correctly if you run it in the root of the
`python/` dir:

```
cd python
black .
```

Optionally: It is recommended to run pylint against new code to protect against bugs
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
