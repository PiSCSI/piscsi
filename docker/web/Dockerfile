ARG GO_VERSION=1.25
ARG OS_VERSION=bullseye

FROM golang:${GO_VERSION}-bookworm AS build

WORKDIR /src

COPY go/go.mod go/go.sum ./go/
WORKDIR /src/go
RUN go mod download \
    && go install google.golang.org/protobuf/cmd/protoc-gen-go@v1.36.11

WORKDIR /src
COPY go go
COPY proto proto

WORKDIR /src/go
RUN apt-get update \
    && apt-get install --yes --no-install-recommends protobuf-compiler \
    && rm -rf /var/lib/apt/lists/* \
    && PATH="$(go env GOPATH)/bin:$PATH" ./proto/generate.sh \
    && CGO_ENABLED=0 go build -tags nomsgpack -ldflags '-s -w' -o /out/piscsi-web ./piscsi-web/cmd/piscsi-web \
    && CGO_ENABLED=0 go build -ldflags '-s -w' -o /out/mock-piscsi ./piscsi-web/cmd/mock-piscsi

FROM debian:${OS_VERSION}-slim

RUN apt-get update \
    && apt-get install --yes --no-install-recommends \
        ca-certificates \
        disktype \
        dosfstools \
        genisoimage \
        hfsutils \
        kpartx \
        man2html \
        openssl \
        unar \
        unzip \
        wget \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd --system pi \
    && useradd --system --create-home --home-dir /home/pi --shell /usr/sbin/nologin --gid pi pi \
    && install -d --owner=pi --group=pi --mode=0777 \
        /home/pi/images \
        /home/pi/shared_files \
        /home/pi/.config/piscsi \
    && install -d --owner=pi --group=pi \
        /home/pi/web/templates \
        /home/pi/web/static

COPY --from=build /out/piscsi-web /usr/local/bin/piscsi-web
COPY --from=build /out/mock-piscsi /usr/local/bin/mock-piscsi
COPY --from=build /src/go/piscsi-web/drive_properties.json /home/pi/drive_properties.json
COPY docker/web/web_start_wrapper.sh /usr/local/bin/web_start_wrapper.sh
RUN chown pi:pi /home/pi/drive_properties.json \
    && chmod 0755 /usr/local/bin/piscsi-web /usr/local/bin/mock-piscsi /usr/local/bin/web_start_wrapper.sh

USER pi
WORKDIR /home/pi

EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/web_start_wrapper.sh"]

HEALTHCHECK --interval=5m --timeout=3s \
  CMD wget --quiet --output-document=/dev/null http://localhost:8080/healthcheck
