class HardwareButton:

    def __init__(self, pca_driver, pin):
        self.pca_driver = pca_driver
        self.pin = pin
        self.state = True
        self.state_interrupt = True
        self.name = "n/a"

    def read(self):
        return self.pca_driver.read_port(self.pin)
