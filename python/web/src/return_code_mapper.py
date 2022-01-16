from rascsi.return_codes import ReturnCodes
from flask_babel import _


class ReturnCodeMapper:

    MESSAGES = {
        ReturnCodes.DELETEFILE_SUCCESS: _(u"File deleted: %(file_path)s"),
        ReturnCodes.DELETEFILE_FILE_NOT_FOUND: _(u"File to delete not found: %(file_path)s"),
        ReturnCodes.RENAMEFILE_SUCCESS: _(u"File moved to: %(target_path)s"),
        ReturnCodes.RENAMEFILE_UNABLE_TO_MOVE: _(u"Unable to move file to: %(target_path)s"),
        ReturnCodes.DOWNLOADFILETOISO_SUCCESS: _(u"Created CD-ROM ISO image with arguments \"%(value)s\""),
        ReturnCodes.DOWNLOADTODIR_SUCCESS: _(u"%(file_name)s downloaded to %(save_dir)s"),
        ReturnCodes.WRITECONFIG_SUCCESS: _(u"Saved configuration file to %(file_name)s"),
        ReturnCodes.WRITECONFIG_COULD_NOT_WRITE: _(u"Could not write to file: %(file_name)s"),
        ReturnCodes.READCONFIG_SUCCESS: _(u"Loaded configurations from: %(file_name)s"),
        ReturnCodes.READCONFIG_COULD_NOT_READ: _(u"Could not read configuration file: %(file_name)s"),
        ReturnCodes.READCONFIG_INVALID_CONFIG_FILE_FORMAT: _(u"Invalid configuration file format"),
        ReturnCodes.WRITEDRIVEPROPS_SUCCESS: _(u"Created properties file: %(file_path)s"),
        ReturnCodes.WRITEDRIVEPROPS_COULD_NOT_WRITE: _(u"Could not write to properties file: %(file_path)s"),
        ReturnCodes.READDRIVEPROPS_SUCCESS: _(u"Read properties from file: %(file_path)s"),
        ReturnCodes.READDRIVEPROPS_COULD_NOT_READ: _(u"Could not read properties from file: %(file_path)s"),
        ReturnCodes.ATTACHIMAGE_COULD_NOT_ATTACH: _(u"Cannot insert an image for %(device_type)s into a "
                                                    u"%(current_device_type)s device"),
    }

    @staticmethod
    def add_msg(payload):
        if "return_code" not in payload:
            return payload

        parameters = payload["parameters"]

        payload["msg"] = _(ReturnCodeMapper.MESSAGES[payload["return_code"]], **parameters)
        return payload
