"""Module implementing the Observer part of the Observer pattern"""

from abc import ABC, abstractmethod


# pylint: disable=too-few-public-methods
class Observer(ABC):
    """Class implementing an abserver"""

    @abstractmethod
    def update(self, updated_object) -> None:
        """Abstract method for updating an observer. Needs to be implemented by subclasses."""
