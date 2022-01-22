from abc import ABC, abstractmethod
from menu.menu import Menu
from rascsi.ractl_cmds import RaCtlCmds


class MenuBuilder(ABC):

    def __init__(self, ractl: RaCtlCmds):
        self._rascsi_client = ractl

    @abstractmethod
    def build(self, name: str, context_object=None) -> Menu:
        pass

    def get_rascsi_client(self):
        return self._rascsi_client

