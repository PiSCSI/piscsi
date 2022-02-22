"""
Module with methods that interact with the Pi's Linux system
"""


class SysCmds:
    """
    Class for commands sent to the Pi's Linux system.
    """
    def get_ip_and_host(self):
        """
        Use a mock socket connection to identify the Pi's hostname and IP address
        """
        from socket import socket, gethostname, AF_INET, SOCK_DGRAM
        host = gethostname()
        sock = socket(AF_INET, SOCK_DGRAM)
        try:
            # mock ip address; doesn't have to be reachable
            sock.connect(('10.255.255.255', 1))
            ip_addr = sock.getsockname()[0]
        except Exception:
            ip_addr = False
        finally:
            sock.close()
        return ip_addr, host
