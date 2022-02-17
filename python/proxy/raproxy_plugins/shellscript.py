from yapsy.IPlugin import IPlugin
import subprocess


class RaProxyShellscriptPlugin(IPlugin):
    def init_hook(self):
        subprocess.call(['./raproxy_plugins/raproxy_init.sh'])

    def detach_hook(self, scsi_id, type, path):
        subprocess.call(['./raproxy_plugins/raproxy_detach.sh',str(scsi_id),str(type),str(path)])

    def eject_hook(self, scsi_id, type, path):
        subprocess.call(['./raproxy_plugins/raproxy_eject.sh',str(scsi_id),str(type),str(path)])

    def attach_hook(self, scsi_id, type, path):
        subprocess.call(['./raproxy_plugins/raproxy_attach.sh',str(scsi_id),str(type),str(path)])
        
