"""Module providing implementations for menu transitions."""

from abc import abstractmethod
from PIL import Image


# pylint: disable=too-few-public-methods
class Transition:
    """Class provides the interface for menu transitions. Must be subclassed."""

    def __init__(self, disp):
        self.disp = disp

    @abstractmethod
    def perform(self, start_image: Image, end_image: Image, transition_attributes=None) -> None:
        """Signature for an abstract transition. Must be implemented in subclasses."""


class PushTransition(Transition):
    """Class implementing a push left/right transition."""

    PUSH_LEFT_TRANSITION = "push_left"
    PUSH_RIGHT_TRANSITION = "push_right"

    def __init__(self, disp):
        super().__init__(disp)
        self.transition_attributes = None

    def perform(self, start_image: Image, end_image: Image, transition_attributes=None):
        """Performs a push transition to the left or right depending on the
        transition attribute 'direction'."""
        direction = {}
        self.transition_attributes = transition_attributes
        if transition_attributes is not None and transition_attributes != {}:
            direction = transition_attributes["direction"]

        transition_image = Image.new("1", (self.disp.width, self.disp.height))

        if direction == PushTransition.PUSH_LEFT_TRANSITION:
            self.perform_push_left(end_image, start_image, transition_image)
        elif direction == PushTransition.PUSH_RIGHT_TRANSITION:
            self.perform_push_right(end_image, start_image, transition_image)
        else:
            self.disp.image(end_image)
            self.disp.show()

    def perform_push_left(self, end_image, start_image, transition_image):
        """Implements a push left transition. Is called by perform depending on the transition
        attribute 'direction'."""
        for x_pos in range(0, 128, self.transition_attributes["transition_speed"]):
            left_region = start_image.crop((x_pos, 0, 128, 64))
            right_region = end_image.crop((0, 0, x_pos, 64))
            transition_image.paste(left_region, (0, 0, 128 - x_pos, 64))
            transition_image.paste(right_region, (128 - x_pos, 0, 128, 64))
            self.disp.display(transition_image)
        self.disp.display(end_image)

    def perform_push_right(self, end_image, start_image, transition_image):
        """Implements a push right transition. Is called by perform depending on the transition
        attribute 'direction'."""
        for x_pos in range(0, 128, self.transition_attributes["transition_speed"]):
            left_region = start_image.crop((0, 0, 128 - x_pos, 64))
            right_region = end_image.crop((128 - x_pos, 0, 128, 64))
            transition_image.paste(left_region, (x_pos, 0, 128, 64))
            transition_image.paste(right_region, (0, 0, x_pos, 64))
            self.disp.display(transition_image)
        self.disp.display(end_image)
