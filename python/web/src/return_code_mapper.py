"""Module for mapping between return codes and translated strings"""

from piscsi.return_codes import ReturnCodes
from flask_babel import _, lazy_gettext


# pylint: disable=too-few-public-methods
class ReturnCodeMapper:
    """Class for mapping between return codes and translated strings"""

    # fmt: off
    MESSAGES = {
        ReturnCodes.DELETEFILE_SUCCESS:
            _("File deleted: %(file_path)s"),
        ReturnCodes.DELETEFILE_FILE_NOT_FOUND:
            _("File to delete not found: %(file_path)s"),
        ReturnCodes.DELETEFILE_UNABLE_TO_DELETE:
            _("Could not delete file: %(file_path)s"),
        ReturnCodes.RENAMEFILE_SUCCESS:
            _("File moved to: %(target_path)s"),
        ReturnCodes.RENAMEFILE_UNABLE_TO_MOVE:
            _("Unable to move file to: %(target_path)s"),
        ReturnCodes.DOWNLOADFILETOISO_SUCCESS:
            _("Created CD-ROM ISO image with arguments \"%(value)s\""),
        ReturnCodes.DOWNLOADTODIR_SUCCESS:
            _("Downloaded file to %(target_path)s"),
        ReturnCodes.WRITEFILE_SUCCESS:
            _("File created: %(target_path)s"),
        ReturnCodes.WRITEFILE_COULD_NOT_WRITE:
            _("Could not create file: %(target_path)s"),
        ReturnCodes.WRITEFILE_COULD_NOT_OVERWRITE:
            _("A file with name %(target_path)s already exists"),
        ReturnCodes.READCONFIG_SUCCESS:
            _("Loaded configurations from: %(file_name)s"),
        ReturnCodes.READCONFIG_COULD_NOT_READ:
            _("Could not read configuration file: %(file_name)s"),
        ReturnCodes.READCONFIG_INVALID_CONFIG_FILE_FORMAT:
            _("Invalid configuration file format"),
        ReturnCodes.READDRIVEPROPS_SUCCESS:
            _("Read properties from file: %(file_path)s"),
        ReturnCodes.READDRIVEPROPS_COULD_NOT_READ:
            _("Could not read properties from file: %(file_path)s"),
        ReturnCodes.ATTACHIMAGE_COULD_NOT_ATTACH:
            _("Cannot insert an image for %(device_type)s into a %(current_device_type)s device"),
        ReturnCodes.EXTRACTIMAGE_SUCCESS:
            _("Extracted %(count)s file(s)"),
        ReturnCodes.EXTRACTIMAGE_NO_FILES_SPECIFIED:
            _("Unable to extract archive: No files were specified"),
        ReturnCodes.EXTRACTIMAGE_NO_FILES_EXTRACTED:
            _("No files were extracted (existing files are skipped)"),
        ReturnCodes.EXTRACTIMAGE_COMMAND_ERROR:
            _("Unable to extract archive: %(error)s"),
        ReturnCodes.UNDER_VOLTAGE_DETECTED:
            _("Potential instability - Under voltage detected - Make sure to use a sufficient "
              "power source (2.5+ amps)."),
        ReturnCodes.ARM_FREQUENCY_CAPPED:
            _("Potential instability - ARM frequency capped - Ensure sufficient airflow/cooling."),
        ReturnCodes.CURRENTLY_THROTTLED:
            _("Potential instability - Currently throttled - Make sure to use a sufficient power "
              "source (2.5+ amps)."),
        ReturnCodes.SOFT_TEMPERATURE_LIMIT_ACTIVE:
            _("Potential instability - Soft-temperature limit active - Ensure sufficient "
              "airflow/cooling."),
        ReturnCodes.UNDER_VOLTAGE_HAS_OCCURRED:
            _("Potential instability - Under voltage has occurred since last reboot.  Make sure "
              "to use a sufficient power source (2.5+ amps)."),
        ReturnCodes.ARM_FREQUENCY_CAPPING_HAS_OCCURRED:
            _("Potential instability - ARM frequency capping has occurred since last reboot.  "
              "Ensure sufficient airflow/cooling."),
        ReturnCodes.THROTTLING_HAS_OCCURRED:
            _("Potential instability - Throttling has occurred since the last reboot.  Make sure "
              "to use a sufficient power source (2.5+ amps)."),
        ReturnCodes.SOFT_TEMPERATURE_LIMIT_HAS_OCCURRED:
            _("Potential instability - Soft temperature limit has occurred since last reboot.  "
              "Ensure sufficient airflow/cooling."),
        }
    # fmt: on

    @staticmethod
    def add_msg(payload):
        """adds a msg key to a given payload with a module return code
        with a translated return code message string."""
        if "return_code" not in payload:
            return payload

        parameters = payload.get("parameters")

        if parameters:
            payload["msg"] = lazy_gettext(
                ReturnCodeMapper.MESSAGES[payload["return_code"]],
                **parameters,
            )
        else:
            payload["msg"] = lazy_gettext(ReturnCodeMapper.MESSAGES[payload["return_code"]])

        return payload
