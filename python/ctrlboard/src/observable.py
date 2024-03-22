"""Module for Observable part of the Observer pattern functionality"""

from typing import List
from observer import Observer


class Observable:
    """Class implementing the Observable pattern"""

    _observers: List[Observer] = []

    def attach(self, observer: Observer):
        """Attaches an observer to an obserable object"""
        self._observers.append(observer)

    def detach(self, observer: Observer):
        """detaches an observer from an observable object"""
        self._observers.remove(observer)

    def notify(self, updated_object):
        """Notifies all observers with a given object parameter"""
        for observer in self._observers:
            observer.update(updated_object)
