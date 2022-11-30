"""Module for test printing events when buttons from the RaSCSI Control Board are pressed"""
import observer
from ctrlboard_hw.hardware_button import HardwareButton
from ctrlboard_hw.encoder import Encoder


# pylint: disable=too-few-public-methods
class CtrlBoardPrintEventHandler(observer.Observer):
    """Class implements a basic event handler that prints button presses from the RaSCSI
    Control Board hardware."""

    def update(self, updated_object):
        if isinstance(updated_object, HardwareButton):
            print(updated_object.name + " has been pressed!")
        if isinstance(updated_object, Encoder):
            print(updated_object.pos)
