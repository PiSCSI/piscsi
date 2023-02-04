# PiSCSI Common Python Module

The common module contains python modules that are shared among multiple Python 
applications such as the OLED or the Web application. It contains shared functionality.
For example, the piscsi python module provides functionality for accessing piscsi through its
protobuf interface and provides convenient classes for that purpose. 

### Usage

To make use of the piscsi python module, it needs to be found by the Python scripts using it. 
This can be achieved in multiple ways. One way is to simply adapt the PYTHONPATH environment
variable to include the common/src directory:

```
PYTHON_COMMON_PATH=${path_to_common_directory}/common/src
export PYTHONPATH=$PWD:${PYTHON_COMMON_PATH}
python3 myapp.py
```

The most interesting functions are likely found in the classes PiscsiCmds and FileCmds. Classes
can be instantiated, for example, as follows 
(assuming that piscsi host, piscsi port and token are somehow retrieved from a command line 
argument):

```
sock_cmd = SocketCmds(host=args.piscsi_host, port=args.piscsi_port)
piscsi_cmd = PiscsiCmds(sock_cmd=sock_cmd, token=args.token)
```

Usage examples can be found in the existing PiSCSI Python applications.
