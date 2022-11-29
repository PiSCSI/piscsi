"""
A minimal wrapper around 'The Unarchiver' command line tools (v1.10.1)
https://theunarchiver.com/command-line
Later versions (untested) available at: https://github.com/MacPaw/XADMaster
"""

import logging
import pathlib

from tempfile import TemporaryDirectory
from re import escape, match
from json import loads, JSONDecodeError
from shutil import move
from util.run import run

FORK_OUTPUT_TYPE_VISIBLE = "visible"
FORK_OUTPUT_TYPE_HIDDEN = "hidden"

FORK_OUTPUT_TYPES = [FORK_OUTPUT_TYPE_VISIBLE, FORK_OUTPUT_TYPE_HIDDEN]


def extract_archive(file_path, **kwargs):
    """
    Extracts files from an archive
    Takes (str) file_path, and kwargs:
    - (list) members - list of (str) files to be extracted (all files are extracted if None)
    - (str) output_dir - directory to place the extracted files
    - (str) fork_output_type - output type for resource forks;
            "visible" for *.rsrc files, "hidden" for ._* files
    Returns (dict) of extracted and skipped members
    """
    members = kwargs.get("members")

    if kwargs.get("output_dir"):
        if not pathlib.Path(kwargs["output_dir"]).is_dir():
            raise ValueError("Argument output_dir must be a directory")
        output_dir = str(pathlib.Path(kwargs["output_dir"]).resolve())
    else:
        output_dir = str(pathlib.Path(file_path).parent.resolve())

    if kwargs.get("fork_output_type"):
        if kwargs["fork_output_type"] not in FORK_OUTPUT_TYPES:
            raise ValueError(
                f"Argument fork_output_type must be one of: {','.join(FORK_OUTPUT_TYPES)} "
            )
        fork_output_type = kwargs["fork_output_type"]
        fork_output_type_args = ["-forks", fork_output_type or FORK_OUTPUT_TYPE_VISIBLE]
    else:
        fork_output_type = None
        fork_output_type_args = []

    with TemporaryDirectory() as tmp_dir:
        unar_args = [
            "-output-directory",
            tmp_dir,
            "-force-skip",
            "-no-directory",
            *fork_output_type_args,
            "--",
            file_path,
        ]

        if members:
            for member in members:
                unar_args.append(escape(member))

        process = run("unar", unar_args)

        if process["returncode"] != 0:
            raise UnarCommandError(f"Non-zero return code: {process['returncode']}")

        unar_result_success = r'^Successfully extracted to "(?P<destination>.+)".$'
        unar_result_no_files = "No files extracted."
        unar_file_extracted = (
            r"^ {2}(?P<path>.+). \(((?P<size>\d+) B)?(?P<types>(dir)?(, )?"
            r"(rsrc)?)\)\.\.\. (?P<status>[A-Z]+)\.$"
        )

        lines = process["stdout"].rstrip("\n").split("\n")

        if lines[-1] == unar_result_no_files:
            raise UnarNoFilesExtractedError

        if match(unar_result_success, lines[-1]):
            extracted_members = []

            for line in lines[1:-1]:
                line_matches = match(unar_file_extracted, line)
                if line_matches:
                    matches = line_matches.groupdict()
                    member = {
                        "name": str(pathlib.PurePath(matches["path"]).name),
                        "path": matches["path"],
                        "size": matches["size"] or 0,
                        "is_dir": False,
                        "is_resource_fork": False,
                        "absolute_path": str(pathlib.PurePath(tmp_dir).joinpath(matches["path"])),
                    }

                    member_types = matches.get("types", "")
                    if member_types.startswith(", "):
                        member_types = member_types[2:].split(", ")
                    else:
                        member_types = member_types.split(", ")

                    if "dir" in member_types:
                        member["is_dir"] = True

                    if "rsrc" in member_types:
                        if not fork_output_type:
                            continue

                        member["is_resource_fork"] = True

                        # Update names/paths to match unar resource fork naming convention
                        if fork_output_type == FORK_OUTPUT_TYPE_HIDDEN:
                            member["name"] = f"._{member['name']}"
                        else:
                            member["name"] += ".rsrc"
                        member["path"] = str(
                            pathlib.PurePath(member["path"]).parent.joinpath(member["name"])
                        )
                        member["absolute_path"] = str(
                            pathlib.PurePath(tmp_dir).joinpath(member["path"])
                        )

                    logging.debug("Extracted: %s -> %s", member["path"], member["absolute_path"])
                    extracted_members.append(member)
                else:
                    raise UnarUnexpectedOutputError(f"Unexpected output: {line}")

            moved = []
            skipped = []
            for member in sorted(extracted_members, key=lambda m: m["path"]):
                source_path = pathlib.Path(member["absolute_path"])
                target_path = pathlib.Path(output_dir).joinpath(member["path"])
                member["absolute_path"] = str(target_path)

                if target_path.exists():
                    logging.info(
                        "Skipping temp file/dir as the target already exists: %s",
                        target_path,
                    )
                    skipped.append(member)
                    continue

                if member["is_dir"]:
                    logging.debug("Creating empty dir: %s -> %s", source_path, target_path)
                    target_path.mkdir(parents=True, exist_ok=True)
                    moved.append(member)
                    continue

                # The parent dir may not be specified as a member, so ensure it exists
                target_path.parent.mkdir(parents=True, exist_ok=True)
                logging.debug("Moving temp file: %s -> %s", source_path, target_path)
                move(str(source_path), str(target_path))
                moved.append(member)

            return {
                "extracted": moved,
                "skipped": skipped,
            }

        raise UnarUnexpectedOutputError(lines[-1])


def inspect_archive(file_path):
    """
    Calls `lsar` to inspect the contents of an archive
    Takes (str) file_path
    Returns (dict) of (str) format, (list) members
    """
    if not pathlib.Path(file_path):
        raise FileNotFoundError(f"File {file_path} does not exist")

    process = run("lsar", ["-json", "--", file_path])

    if process["returncode"] != 0:
        raise LsarCommandError(f"Non-zero return code: {process['returncode']}")

    try:
        archive_info = loads(process["stdout"])
    except JSONDecodeError as error:
        raise LsarOutputError(f"Unable to read JSON output from lsar: {error.msg}") from error

    members = [
        {
            "name": pathlib.PurePath(member.get("XADFileName")).name,
            "path": member.get("XADFileName"),
            "size": member.get("XADFileSize"),
            "is_dir": member.get("XADIsDirectory"),
            "is_resource_fork": member.get("XADIsResourceFork"),
            "raw": member,
        }
        for member in archive_info.get("lsarContents", [])
    ]

    return {
        "format": archive_info.get("lsarFormatName"),
        "members": members,
    }


class UnarCommandError(Exception):
    """Command execution was unsuccessful"""

    pass


class UnarNoFilesExtractedError(Exception):
    """Command completed, but no files extracted"""


class UnarUnexpectedOutputError(Exception):
    """Command output not recognized"""


class LsarCommandError(Exception):
    """Command execution was unsuccessful"""


class LsarOutputError(Exception):
    """Command output could not be parsed"""
