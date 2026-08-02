# Docker Environment for Development and Testing

⚠️ **Important:** The Docker environment is unable to connect to the PiSCSI board and is
intended for development and testing purposes only. To setup PiSCSI on a Raspberry Pi
refer to the [setup instructions](https://github.com/PiSCSI/piscsi/wiki/Setup-Instructions)
on the wiki instead.

## Introduction

This documentation covers developing and testing the Go web UI.

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

To stop the containers, press *Ctrl + C*, or run `docker compose stop` 
from another terminal.

## Environment Variables

The following environment variables are available when using Docker Compose:

| Environment Variable | Default  |
| -------------------- |----------|
| `OS_VERSION`         | bullseye |
| `WEB_HTTP_PORT`      | 8080     |
| `BACKEND_HOST`       | backend  |
| `BACKEND_PORT`       | 6868     |
| `BACKEND_PASSWORD`   | *[None]* |
| `BACKEND_LOG_LEVEL`  | debug    |
| `PISCSI_DOCKER_UID`  | 1000     |
| `PISCSI_DOCKER_GID`  | 1000     |

**Examples:**

Run Debian "bullseye":
```
OS_VERSION=bullseye docker compose up
```

The Docker entrypoint generates an ephemeral session key for local development.
Set `SESSION_KEY` yourself if browser sessions must survive a container restart.

Run the Go web-client test suite with:

```
docker compose --profile webui-tests run --rm go-test
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
PiSCSI or making changes to the Go web client or its runtime dependencies.

```
docker compose up --build
```

### Open a Shell on a Running Container

Run the following command, replacing `[CONTAINER]` with `backend` or `web`.

```
docker compose exec [CONTAINER] bash
```

### Setup Live Editing for the Web UI

Use a `docker-compose.override.yml` to mount the local `go` directory into the
build workspace used by the `go-test` service.

The web binary embeds templates and static assets, so rebuild and restart the
`web` service after editing Go web-client files.

**Example:**
```
services:
  go-test:
    volumes:
      - ../go:/src/go:delegated
```

### Connect the Web UI to a Real PiSCSI

This can be useful for testing, but there are some caveats, e.g. the PiSCSI and the
web UI will be accessing separate `images` directories.

```
BACKEND_HOST=foo BACKEND_PASSWORD=bar docker compose up
```
