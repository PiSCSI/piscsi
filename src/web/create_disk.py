from machfs import Volume, Folder, File

from settings import *


# Build a cd and attempt to fix resource forks if known
def make_cd(file_path, file_type, file_creator):
    with open(file_path, "rb") as f:
        file_bytes = f.read()
    file_name = file_path.split('/')[-1]
    file_suffix = file_name.split('.')[-1]

    if file_type is None and file_creator is None:
        if file_suffix.lower() == 'sea':
            file_type = '.sea'
            file_creator = 'APPL'

    v = Volume()
    v.name = "TestName"

    v['Folder'] = Folder()

    v['Folder'][file_name] = File()
    v['Folder'][file_name].data = file_bytes
    v['Folder'][file_name].rsrc = b''
    if not (file_type is None and file_creator is None):
        v['Folder'][file_name].type = bytearray(file_type)
        v['Folder'][file_name].creator = bytearray(file_creator)

    padding = (len(file_bytes) % 512) + (512 * 50)
    print("mod", str(len(file_bytes) % 512))
    print("padding " + str(padding))
    print("len " + str(len(file_bytes)))
    print("total " + str(len(file_bytes) + padding))
    with open(base_dir + 'test.hda', 'wb') as f:
        flat = v.write(
            size=len(file_bytes) + padding,
            align=512,  # Allocation block alignment modulus (2048 for CDs)
            desktopdb=True,  # Create a dummy Desktop Database to prevent a rebuild on boot
            bootable=False,  # This requires a folder with a ZSYS and a FNDR file
            startapp=('Folder', file_name),  # Path (as tuple) to an app to open at boot
        )
        f.write(flat)

    return base_dir + 'test.hda'
