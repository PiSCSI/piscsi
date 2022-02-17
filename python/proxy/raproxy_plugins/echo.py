from yapsy.IPlugin import IPlugin


class RaProxyEchoPlugin(IPlugin):
    def init_hook(self):
        print("RASCSI proxy echo plugin: init")

    def detach_hook(self, scsi_id, type, path):
        print("detaching from scsi id: " + str(scsi_id))
        print("detaching file type   : " + str(type))
        print("detaching file        : " + str(path))

    def eject_hook(self, scsi_id, type, path):
        print("ejecting from scsi id : " + str(scsi_id))
        print("ejecting file type    : " + str(type))
        print("ejecting file         : " + str(path))

    def attach_hook(self, scsi_id, type, path):
        print("attaching on scsi id  : " + str(scsi_id))
        print("attaching file type   : " + str(type))
        print("attaching file        : " + str(path))
        
