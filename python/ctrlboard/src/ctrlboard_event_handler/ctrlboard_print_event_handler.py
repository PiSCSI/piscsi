import observer
from ctrlboard_hw.hardware_button import HardwareButton
from ctrlboard_hw.encoder import Encoder


class CtrlBoardPrintEventHandler(observer.Observer):
    def update(self, updated_object):
        if isinstance(updated_object, HardwareButton):
            print(updated_object.name + " has been pressed!")
        if isinstance(updated_object, Encoder):
            print(updated_object.pos)
