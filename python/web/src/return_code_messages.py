from rascsi.return_codes import ReturnCodes
from flask_babel import _


class ReturnCodeMapper:

    MESSAGES = {
        ReturnCodes.DELETEFILE_SUCCESS: _(u"File deleted: %(file_path)s"),
        ReturnCodes.DELETEFILE_FILE_NOT_FOUND: _(u"File to delete not found: %(file_path)s"),
        ReturnCodes.RENAMEFILE_SUCCESS: _(u"File moved to: %(target_path)s"),
        ReturnCodes.RENAMEFILE_UNABLE_TO_MOVE: _(u"Unable to move file to: %(target_path)s"),
        ReturnCodes.DOWNLOADFILETOISO_SUCCESS: _(u"Created CD-ROM ISO image with arguments \"%(value)s\""),
        ReturnCodes.DOWNLOADTODIR_SUCCESS: _(u"%(file_name)s downloaded to %(save_dir)s")
    }

    @staticmethod
    def add_msg(payload):
        if payload["return_code"] is None:
            return payload

        parameters = payload["parameters"]

        payload["msg"] = _(ReturnCodeMapper.MESSAGES[payload["return_code"]], **parameters)
        return payload
