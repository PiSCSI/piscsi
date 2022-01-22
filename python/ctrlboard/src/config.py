class CtrlboardConfig:
    ROTATION = 0
    WIDTH = 128
    HEIGHT = 64
    LINES = 8
    TOKEN = ""
    BORDER = 5
    RASCSI_HOST = "localhost"
    RASCSI_PORT = "6868"

    def __str__(self):
        result = "rotation: " + str(self.ROTATION) + "\n"
        result += "width: " + str(self.WIDTH) + "\n"
        result += "height: " + str(self.HEIGHT) + "\n"
        result += "lines: " + str(self.LINES) + "\n"
        result += "border: " + str(self.BORDER) + "\n"
        result += "rascsi host: " + str(self.RASCSI_HOST) + "\n"
        result += "rascsi port: " + str(self.RASCSI_PORT) + "\n"

        return result
