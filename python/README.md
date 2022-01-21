# RaSCSI Python Apps 

This directory contains Python-based clients for RaSCSI as well as common 
packages that are shared among the clients. 

The following paragraphs in this README contain instructions that are shared 
among all Python apps.

### Static analysis with pylint

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