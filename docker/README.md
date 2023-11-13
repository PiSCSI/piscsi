# Docker Environment for Development and Testing

⚠️ **Important:** The Docker environment is unable to connect to the PiSCSI board and is
intended for development and testing purposes only. To setup PiSCSI on a Raspberry Pi
refer to the [setup instructions](https://github.com/PiSCSI/piscsi/wiki/Setup-Instructions)
on the wiki instead.

## Introduction

This documentation currently focuses on using Docker for developing and testing the web UI.

Additions, amendments and contributions for additional workflows are most welcome.

## Getting Started

The easiest way to launch a new environment is to use Docker Compose.

```
cd docker
docker compose up
```

Containers will be built and started for the PiSCSI server and the web UI.

The web UI can be accessed at:

* http://localhost:8080
* https://localhost:8443

To stop the containers, press *Ctrl + C*, or run `docker compose stop` 
from another terminal.

## Environment Variables

The following environment variables are available when using Docker Compose:

| Environment Variable | Default  |
| -------------------- |----------|
| `OS_VERSION`         | bullseye |
| `WEB_HTTP_PORT`      | 8080     |
| `WEB_HTTPS_PORT`     | 8443     |
| `WEB_LOG_LEVEL`      | info     |
| `BACKEND_HOST`       | backend  |
| `BACKEND_PORT`       | 6868     |
| `BACKEND_PASSWORD`   | *[None]* |
| `BACKEND_LOG_LEVEL`  | debug    |
| `RESET_VENV`         | *[None]* |

**Examples:**

Run Debian "bullseye":
```
OS_VERSION=bullseye docker compose up
```

Start the web UI with the log level set to debug:
```
WEB_LOG_LEVEL=debug docker compose up
```

Force resetting & reinstalling Python web `venv` directory:

```
RESET_VENV=1 docker compose up
```

## Volumes

When using Docker Compose the following volumes will be mounted automatically:

| Local Path              | Container Path           |
| ----------------------- | ------------------------ |
| docker/volumes/images/  | /home/pi/images/         |
| docker/volumes/config/  | /home/pi/.config/piscsi/ |


## How To

### Rebuild Containers

You should rebuild the container images after checking out a different version of
PiSCSI or making changes which affect the environment at build time, e.g. 
`easyinstall.sh`.

```
docker compose up --build
```

### Open a Shell on a Running Container

Run the following command, replacing `[CONTAINER]` with `backend` or `web`.

```
docker compose exec [CONTAINER] bash
```

### Setup Live Editing for the Web UI

Use a `docker-compose.override.yml` to mount the local `python` directory to
`/home/pi/piscsi/python/` in the `web` container.

Any changes to *.py files on the host computer (i.e. in your IDE) will trigger
the web UI process to be restarted in the container.

**Example:**
```
services:
  web:
    volumes:
      - ../python:/home/pi/piscsi/python:delegated
```

### Connect the Web UI to a Real PiSCSI

This can be useful for testing, but there are some caveats, e.g. the PiSCSI and the
web UI will be accessing separate `images` directories.

```
BACKEND_HOST=foo BACKEND_PASSWORD=bar docker compose up
```
