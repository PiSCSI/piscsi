from yapsy.IPlugin import IPlugin
from pathlib import Path
import configparser
import os
import subprocess


class RaProxyAutomountPlugin(IPlugin):
    def detach(self, scsi_id, type, path):
        file = os.path.basename(str(path))
        automount_path = Path(self.config['main']['automount_directory']).joinpath(file)
        automount_image_config_file = str(path) + self.config['main']['automount_extension']
        if not Path(automount_image_config_file).is_file():
            print("RASCSI proxy automount image config file [" + automount_image_config_file + "] not found. Skipping "
                                                                                               "mount action.")
            return
        automount_image_config = configparser.ConfigParser()
        with open(automount_image_config_file) as stream:
            automount_image_config.read_string("[main]\n" + stream.read())

        if not automount_image_config.has_option("main","mount_options"):
            print("RASCSI proxy automount image config file is invalid and/or does not have a valid key ["
                  "mount_options]. Skipping mount action.")
            return
            
        if not automount_path.exists():
            try:
                automount_path.mkdir(parents=True, exist_ok=True)
            except Exception as e:
                print(e)

        try:
            if self.config['main']['use_sudo']:
                command = [self.config['main']['sudo'],self.config['main']['mount'], path, str(f'{automount_path}/')]
#                print(command)
                return_code = subprocess.call(command)
            else:
                command = [self.config['main']['mount'], path, str(f'{automount_path}/')]
#                print(command)
                return_code = subprocess.call(command)
#            print("return code: " + str(return_code))
            if return_code != 0:
                raise Exception('mounting ' + str(path) + ' failed on detach command. Unknown state!')
            print("RASCSI proxy automount detach/eject action: image [" + str(path) + "] has been mounted to [" + str(f'{automount_path}/') + "].")
        except Exception as e:
            print(e)       
        
    def attach(self, scsi_id, type, path):
        file = os.path.basename(str(path))
        automount_path = Path(self.config['main']['automount_directory']).joinpath(file)       
        if not automount_path.exists():
            print("RASCSI proxy automount image mount directory [" + str(automount_path) + "] not found. Probably not "
                                                                                           "mounted. Skipping unmount"
                                                                                           " action.")
            return

        if len(os.listdir(str(automount_path)) ) > 0:
            try:
                if self.config['main']['use_sudo']:
                    command = [self.config['main']['sudo'],self.config['main']['umount'],'-l', str(f'{automount_path}/')]
#                    print(command)
                    return_code = subprocess.call(command)
                else:
                    command = [self.config['main']['umount'],'-l', str(f'{automount_path}/')]
#                    print(command)
                    return_code = subprocess.call(command)
#                print("return code: " + str(return_code))
                if return_code != 0:
                    raise Exception('Umounting ' + str(path) + ' failed on attach command. Unknown state!')

                print("RASCSI proxy automount attach/insert action: image [" + str(path) + "] has been unmounted from [" + str(f'{automount_path}/') + "].")

            except Exception as e:
                print(e)

        try:
            automount_path.rmdir()
        except Exception as e:
            print(e)
                
    def init_hook(self):
        print("RASCSI proxy automount plugin: init")

        self.config = configparser.ConfigParser()

        with open('./raproxy_plugins/automount.ini') as stream:
            self.config.read_string("[main]\n" + stream.read())
        
        path = Path(self.config['main']['automount_directory'])
        if not path.exists():
            path.mkdir(parents=True, exist_ok=True)

    def detach_hook(self, scsi_id, type, path):
        self.detach(scsi_id, type, path)
#        print("automount path: " + str(path))
#        print("detaching from scsi id: " + str(scsi_id))
#        print("detaching file type   : " + str(type))
#        print("detaching file        : " + str(path))

    def eject_hook(self, scsi_id, type, path):
        self.detach(scsi_id, type, path)
#        print("automount path: " + str(path))
#        print("ejecting from scsi id : " + str(scsi_id))
#        print("ejecting file type    : " + str(type))
#        print("ejecting file         : " + str(path))

    def attach_hook(self, scsi_id, type, path):
        self.attach(scsi_id, type, path)
#        print("automount path: " + str(path))
#        print("attaching on scsi id  : " + str(scsi_id))
#        print("attaching file type   : " + str(type))
#        print("attaching file        : " + str(path))

    def insert_hook(self, scsi_id, type, path):
        self.attach(scsi_id, type, path)
#        print("automount path: " + str(path))
#        print("inserting on scsi id  : " + str(scsi_id))
#        print("inserting file type   : " + str(type))
#        print("inserting file        : " + str(path))

