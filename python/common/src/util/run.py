"""
Utility module for running system commands with basic logging
"""

import asyncio
import logging


def run(program, args=None):
    """Run a command and return its output"""
    return asyncio.run(run_async(program, args))


async def run_async(program, args=None):
    """Run a command in the background"""
    proc = await asyncio.create_subprocess_exec(
        program, *args, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE
    )

    stdout, stderr = await proc.communicate()

    logging.info(
        'Executed command "%s %s" with status code %d',
        program,
        " ".join(args),
        proc.returncode,
    )

    if stdout:
        stdout = stdout.decode()
        logging.debug(stdout)

    if stderr:
        stderr = stderr.decode()
        logging.warning(stderr)

    return {
        "returncode": proc.returncode,
        "stdout": stdout,
        "stderr": stderr,
    }
