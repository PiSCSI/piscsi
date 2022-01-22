from typing import List
from observer import Observer


class Observable:
    _observers: List[Observer] = []

    def attach(self, observer: Observer):
        self._observers.append(observer)

    def detach(self, observer: Observer):
        self._observers.remove(observer)

    def notify(self, updated_object):
        for observer in self._observers:
            observer.update(updated_object)