"""
Linux interrupt handling module
"""

import signal


class GracefulInterruptHandler:
    """
    Class for handling Linux signal interrupts
    """

    def __init__(self, signals=(signal.SIGINT, signal.SIGTERM)):
        self.signals = signals
        self.original_handlers = {}
        self.interrupted = False
        self.released = False

    def __enter__(self):
        for sig in self.signals:
            self.original_handlers[sig] = signal.getsignal(sig)
            signal.signal(sig, self.handler)

        return self

    def handler(self, signum, frame):
        self.release()
        self.interrupted = True

    def __exit__(self, exception_type, exception_value, traceback):
        self.release()

    def release(self):
        if self.released:
            return False

        for sig in self.signals:
            signal.signal(sig, self.original_handlers[sig])

        self.released = True
        return True
